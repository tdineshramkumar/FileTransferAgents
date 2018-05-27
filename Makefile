all: fileserver fileclient

fileserver: fileserver.c
	gcc fileserver.c -o fileserver
	
fileclient: fileclient.c
	gcc fileclient.c -o fileclient -lpthread 
	
clean:
	rm fileclient fileserver -f
