#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <fstream>

using boost::asio::ip::tcp;
using boost::system::error_code;

void log(auto const&... args)
{
	(std::cout << ... << args) << std::endl;
}
class Session : public std::enable_shared_from_this<Session>
{
  public:
    Session(tcp::socket socket) : socket_(std::move(socket)) {}
    void start() { do_read(); }

  private:
    void do_read() 
	{
        //log("Server reads.");
        auto self = shared_from_this();
        socket_.async_read_some(boost::asio::buffer(data_), [this, self](error_code ec, size_t length) {
            //log("Session read: ", ec.message(), " ", length);
            if (!ec || (length && ec == boost::asio::error::eof))
			{
                receive_data(length);
				return;
			}			
        });
    }

    void do_write(size_t length) 
	{
        //log("Server writes: ", response_);
        auto self = shared_from_this();
        async_write(socket_, boost::asio::buffer(response_), [this, self](error_code ec, size_t length) {
            //log("Session write: ", ec.message(), " ", response_.length());
            if (!ec)
                do_read();
        });
    }
	
	void receive_data(size_t length) 
	{
		msg_ += std::string(data_.data());
		std::fill(std::begin(data_), std::end(data_), '\0');
				
		if(total_sz_ == 0)
		{
			std::string delimiter = ":";
			size_t pos;
			std::string token;
			if ((pos = msg_.find(delimiter)) != std::string::npos)
				total_sz_ = (size_t)atoi(msg_.substr(0, pos).c_str());
		}		
		if(total_sz_ > 0 && msg_.length()>total_sz_)
		{
			action();
			return;
		}			
		do_read();
    }
		
	bool save(std::string& uuid, std::string& chunk, std::string& content)
	{
		std::filesystem::path pdir{uuid};
		if(!std::filesystem::exists(pdir))
			std::filesystem::create_directory(pdir);		
		
		pdir /= chunk;
		
		std::ofstream ofs(pdir);
		ofs << msg_; 
		ofs.close();
		if (!ofs)
			return false;

		return true;
	}
	
	bool load(std::string& uuid, std::vector<std::string>& vhorcruxes)
	{
		std::filesystem::path pdir{uuid};
		if(!std::filesystem::exists(pdir))
			return false;
					
		for (const auto& entry : std::filesystem::directory_iterator(pdir)) {
			if(entry.is_regular_file())
				vhorcruxes.emplace_back(entry.path().string());
		}
				
		return true;
	}
	
	void action() 
	{		
		std::string delimiter = ":";
				
		std::string action;
		std::string uuid_file;
		
		//size
		size_t pos = std::string::npos;
		if ((pos = msg_.find(delimiter)) != std::string::npos)
		{	
			//log("Size request: ", msg_.substr(0, pos));
			msg_.erase(0, pos + delimiter.length());
		}
		//action
		if ((pos = msg_.find(delimiter)) != std::string::npos)
		{
			action = msg_.substr(0, pos);
			log("Action: ", action);
			msg_.erase(0, pos + delimiter.length());
		}
		if(action == "save") 
		{			
			//uuid_file
			if ((pos = msg_.find(delimiter)) != std::string::npos)
			{
				uuid_file = msg_.substr(0, pos);
				log("UUID: ", uuid_file);
				msg_.erase(0, pos + delimiter.length());
			}
			std::string id_chunk;
			//chunk_id
			if ((pos = msg_.find(delimiter)) != std::string::npos)
			{
				std::stringstream ssuuid; ssuuid << std::setw(3) << std::setfill('0') << msg_.substr(0, pos);
				id_chunk = ssuuid.str();
				log("Chunk id: ", id_chunk);
				msg_.erase(0, pos + delimiter.length());
			}
						
			if(uuid_file.empty() || id_chunk.empty())
				response_ = "-1";
			else
			{
				if (save(uuid_file, id_chunk, msg_))
					response_ = "0";
			}
			
			do_write(response_.length());
		}
		else if(action == "load")
		{						
			uuid_file = msg_;
			log("Loading file uuid: ", msg_);
			
			std::vector<std::string> horcruxes;
			if (!load(uuid_file, horcruxes))
			{
				response_ = "-1";
				do_write(response_.length());
				socket_.close();
			}
			else
			{ 
				bool ok = true;
				size_t total = horcruxes.size();
				for(auto& h: horcruxes)
				{
					std::string content;
					std::ifstream fs(std::filesystem::path{h});
					fs.seekg(0, std::ios_base::end);
					auto size = fs.tellg();
					fs.seekg(0);
					content.resize(static_cast<size_t>(size));
					fs.read(&content[0], static_cast<std::streamsize>(size));
				
					std::string index = h.substr(h.find_last_of("/")+1);
				
					if (content.empty())
					{
						response_ = "-1";
						ok = false;
						do_write(response_.length());
						break;
					}
					else
					{
						std::stringstream ss;
						ss << "[HC]" << "0:" << index << ":" << total << ":" << content << "[/HC]";

						size_t sz = ss.str().length();
						std::stringstream sssize; sssize << sz;
						std::stringstream tosend; tosend << sz + sssize.str().length() << ":" << ss.str();
						response_ = ss.str();
					}

					do_write(response_.length());
				}			
				if(ok)
					socket_.close();
			}
		}		
		
		msg_.clear();
		total_sz_ = 0;			
	}
			
    tcp::socket socket_;
    std::array<char, 1024> data_;
	std::string msg_;
	size_t total_sz_;
	
	std::string response_;
};

class HorcruxServer {
  public:
    HorcruxServer(boost::asio::any_io_executor ex, boost::asio::ip::address ipAddress, short port)
        : acceptor_(ex, tcp::endpoint(ipAddress, port)) {
        do_accept();
    }

    void stop() {
        post(acceptor_.get_executor(), [this] {
            acceptor_.cancel(); /* or close() */
        });
    }

  private:
    void do_accept() {
        acceptor_.async_accept(                     
            make_strand(acceptor_.get_executor()),
            [this](error_code ec, tcp::socket socket) {
                if (ec)
                    log("Accept: ", ec.message());
                if (!ec) {
                    log("Accepted ", socket.remote_endpoint());
                    std::make_shared<Session>(std::move(socket))->start();

                    do_accept();
                }
            });
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {

    std::cout << std::fixed << std::setprecision(5);
	if (argc != 2)
	{
		log("Correct sythax:\n\n	./server port \n");
		return -1;
	}

	int port;
	try
	{
		port = boost::lexical_cast<int>(argv[1]);
	}
	catch (...)
	{
		log("Error parsing arguments");
		return -1;
	}
    auto const ipAddress = boost::asio::ip::address_v4::loopback();
	
	log("Starting Horcrux server on port ", port, "...");
	boost::asio::io_context io_context;
    HorcruxServer srv(io_context.get_executor(), ipAddress, port);
	
	io_context.run();
		
	srv.stop();    
	
  return 0;
}