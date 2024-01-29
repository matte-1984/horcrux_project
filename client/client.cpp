#include <iostream>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <filesystem>
#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.

namespace asio = boost::asio;
using boost::asio::ip::tcp;
using boost::system::error_code;

void log(auto const&... args) 
{
	(std::cout << ... << args) << std::endl;
}

std::string file_uuid() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}

bool save(tcp::socket& socket, std::string fname, int nchunks, std::string &uuid)
{
	uuid = file_uuid();
	log("File identifier:  ", uuid);
	
    //calculate file size with std::filesystem
	std::filesystem::path p{ fname };
    int file_size =  0;
	try
	{
		file_size = std::filesystem::file_size(p);
	}
	catch (std::filesystem::filesystem_error& e)
	{
		log("Unable to get file size: ", p.string());
		return false;
	}
			
    //calculate chunk size
    long chunk_size;
    chunk_size = (int)ceil((double)file_size / nchunks);
    //log("Total files after split: ", nchunks, "...Processing...");

	//open file
    FILE *fp_read = fopen(fname.c_str(), "rb");
	int remaining_size = file_size;
		    
    //Allocate chunk buffer
	char *chunk_buf = new char[chunk_size];
	//Chunk creation
	bool save_result = true;
	for (int index = 0; index < nchunks; index++) 
	{		
		//Current chunk size
		long current_chunk_sz = remaining_size < chunk_size ? remaining_size : chunk_size;
		//Read data from file 
		fread(chunk_buf, current_chunk_sz, 1, fp_read);
		//Update remaining size
		remaining_size -= current_chunk_sz;

		//log("> Sending chunck ", index,", size ",current_chunk_sz);	
		
		std::stringstream msg;
		msg << "save:"<< uuid << ":" << index << ":" << std::string(chunk_buf, current_chunk_sz);
						
		size_t sz = msg.str().length();		
		std::stringstream sssize; sssize << sz;
		std::stringstream tosend; tosend << sz + sssize.str().length() << ":" << msg.str();
		
        //write
		boost::system::error_code error;
        std::string const request = tosend.str();
        write(socket, asio::buffer(request), error);
		if( !error ) {
			log("+ Client sent chuck ", index);
		}
		else
			log("- Send failed");
            
        
        //read 
        std::array<char, 1024> reply;
        size_t n = socket.read_some(asio::buffer(reply));
		if (std::string_view(reply.data(), n) == "0")
			save_result = true;
		else
			break;
	}
	delete[] chunk_buf;
		
	return save_result;
}
bool load(tcp::socket& socket, std::string uuid, std::string fname) {
	
	std::stringstream ssreq;
	ssreq << "load:"<< uuid;
					
	size_t sz = ssreq.str().length();	
	std::stringstream sssize; sssize << sz;
	std::stringstream tosend; tosend << sz + sssize.str().length() << ":" << ssreq.str();

	boost::system::error_code error;
	std::string const request = tosend.str();
	write(socket, asio::buffer(request), error);
	if( !error )
	{
		log("Client requests file: ", uuid);
	}
	else
		log("Request fails: ", error.message());
		
	//wait response	
	int total = -1; int index = -1;
	std::vector<std::string> vhorcruxes;
	int loaded = 0;
	
	std::string delimiter = ":";	

	std::array<char, 50> reply;
	std::string msg;
	size_t hc_sz = 0;
	bool to_save = false;


	std::string starting_str = "[HC]";
	std::string end_str = "[/HC]";
	
	while (1)
	{
		boost::system::error_code err;
		size_t n = socket.read_some(asio::buffer(reply), err);

		if (err == boost::asio::error::eof)
			break;
		msg += std::string(reply.data(), n);
		
		size_t pos;
		std::string horcrux;
		if ((pos = msg.find(end_str)) != std::string::npos)
		{
			horcrux = msg.substr(0, pos);
			msg.erase(0, pos + end_str.length());
		}
		if (!horcrux.empty())
		{
			//check data
			if ((pos = horcrux.find(starting_str)) == std::string::npos)
			{
				log("Horcrux data corrupted, unable to load");
				break;
			}
			horcrux = horcrux.substr(pos + starting_str.length());

			std::string code;
			if ((pos = horcrux.find_first_of(":")) != std::string::npos)
			{
				code = horcrux.substr(0, pos).c_str();
				log("Status: ", code);
				horcrux.erase(0, pos + delimiter.length());
			}
			if (code != "0")
			{
				log("Horcrux corrupted, unable to load");
				break;
			}

			//index
			if ((pos = horcrux.find_first_of(":")) != std::string::npos)
			{
				index = atoi(horcrux.substr(0, pos).c_str());
				horcrux.erase(0, pos + delimiter.length());
			}
			//total
			if ((pos = horcrux.find_first_of(":")) != std::string::npos)
			{
				total = atoi(horcrux.substr(0, pos).c_str());
				vhorcruxes.resize(total);
				horcrux.erase(0, pos + delimiter.length());
			}

			if (index >= 0 && index < vhorcruxes.size())
			{
				vhorcruxes[index] = horcrux;
				loaded++;
			}

			if (loaded == total)
			{
				to_save = true;
				break;
			}
		}
	}		
	if (to_save)
	{
		std::filesystem::path pdir{ fname };
		std::ofstream ofs(pdir);
		for (auto& h : vhorcruxes)
			ofs << h;
		ofs.close();

		log("File load successfully into file: ", fname);
	}
	return to_save;
}
int main(int argc, char * argv[])
{
		
	std::string port, action, fname, uuid, out;
	int chunks = -1;	
   
    // Parsing arguments
 	bool args_ok = true;
	for (size_t index = 0; index < argc-1; index++)
	{
		std::string s = argv[index];
		if (s == "-h" || s == "-help")
			args_ok = false;
		if (s == "-p")
			port = argv[index + 1];
		if (s == "-a")
			action = argv[index + 1];
		if (s == "-f")
			fname = argv[index + 1];
		if (s == "-n")
		{
			try
			{
				chunks = boost::lexical_cast<int>(argv[index + 1]);
			}
			catch (...)
			{
				chunks = 1;
			}
		}
		if (s == "-uuid")
			uuid = argv[index + 1];
		if (s == "-o")
			out = argv[index + 1];
    }
	if(action.empty() || port.empty()) 
		args_ok = false;
	if(action == "save" && (fname.empty() || chunks <0))
		args_ok = false;
	if(action == "load" && (uuid.empty() || out.empty()))
		args_ok = false;

	if(!args_ok)
	{
		log("Correct sythax: \n ./horcrux -p port -a save -n number_of_chunks -f filename \n ./horcrux -p port -a load -uuid uuid_file -o output");
		return 0;
	}

    auto const address = asio::ip::address_v4::loopback();

    asio::io_context io_context;
    tcp::socket socket(io_context);
    tcp::resolver resolver(io_context);
    connect(socket, resolver.resolve(address.to_string(), port));
	
	if(action == "save")
	{
		std::string uuid;
		if(save(socket, fname, chunks, uuid))
			log("File save successfully: ", uuid);
		else
			log("Error saving file ", fname);
	}
	else if(action == "load")
	{
		load(socket, uuid, out);
	}
		
    return 0;
}