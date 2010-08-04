If you are running a server, placing map (*.cgz) and config (*.cfg) files in this directory allows 
them to be automatically loaded into ram by the server if the map is chosen. You should use this 
for when you want to add custom maps into your map rotation.


If you would like your server to store all maps that are sent to the server by clients, so if the 
map is loaded again, there is no need for the clients to send a map file, create this folder: 
/packages/maps/servermaps/incoming


Note: All clients will automatically getmap the map files (if enabled), these features allow you 
to make sure that users don't neccesarally need to sendmap the map files.