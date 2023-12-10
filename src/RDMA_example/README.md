rdma server
    listens for rdma client
     creates a server buffer
rdma client
    connects with server
    gets data from server buffer


combine it with redis
redis server    
    listens for redis client and internally listens to a rdma client
    pushes stuff to the redis server buffer and the rdma buffer

redis client
    connects with server
    tries to get a key, the server should read from the rdma buffer and returns
