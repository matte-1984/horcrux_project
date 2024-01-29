------------------------------------------------------------------------------
THE HORCRUX PROJECT
------------------------------------------------------------------------------
A simple Client/Server tool to split and save copies of files into filesystem.

------------------------------------------------------------------------------
CREATE USING Dockerfile
------------------------------------------------------------------------------
The Dockerfile into the main folder of the project permits to create a image
file configured with the enviroment able to compile and run the applications.

Into a terminal move to the project directory and run the following command to
create the image file:

    docker build -t horcrux_project .

After that run a container and connect to the bash:

    docker run -it --name="test_horcrux" horcrux_project bash

or connect to the already running one:

    docker exec -it test_horcrux bash

 The working directory of the container is set to the current project folder
 where is possible to run server or client application.

------------------------------------------------------------------------------
RUN SERVER APPLICATION
------------------------------------------------------------------------------
The correct synthax to run the server is:

    .horcrux_server port_value

Example:
    
    ./server/horcrux_server 1234
------------------------------------------------------------------------------
RUN CLIENT APPLICATION
------------------------------------------------------------------------------
With the client is possible to perform two different actions: 

- save a file
  The client split the file into N horcruxes, assign a unique identifier and
  send them to server.
  The server return back a status code related to the state of operations.
  
        ./horcrux -p port_value -a save -n numeber_of_chunks -f filename

  Example:

        ./client/horcrux -p 1234 -a save -n 4 -f ./client/test_file.txt
  
- load a file
  In the load action the client request a file, specifing the identifier, and
  receive back the N chunks from the server. Once received all files, the
  client tries to joint the file back into a new file.

        ./horcrux -p port_value -a save -uuid file_identifier -o out_filename

  Example:

        ./client/horcrux -p 1234 -a load -uuid 340532f7-52f7-43bd-89df-539d98fd3d9e -o merge.txt
