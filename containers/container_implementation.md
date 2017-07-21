# Container Implementation Details
The following is relevant for the implementation of model containers in different languages. 
Model containers must implement functionality consistent with this information.

## Connection Lifecycle
The connection lifecycle defines the socket construction, destruction, and state-based behavior for a single Clipper session. We define a session as a sustained connection between a container and Clipper. Key points:

- Every session requires a distinct socket. When a session ends, a container must create a new socket in order to reconnect to Clipper.
- Sessions are initiated and sustained by a heartbeating process. Containers send the first heartbeat message and wait until they receive a response from Clipper. The initial detection of a heartbeat response from Clipper marks the creation of a successful connection, and the loss of a heartbeat indicates that a connection is broken.
    
### RPC Message Types 
Model containers communicate with Clipper using RPC messages of several types. Each RPC message is a [multi-part ZeroMQ message](http://zguide.zeromq.org/php:chapter2#toc11) beginning with an [empty ZeroMQ frame](http://zguide.zeromq.org/php:chapter3#The-Simple-Reply-Envelope). Each message contains a **Message Type** field, encoded as an **unsigned integer**, that specifies one of the following types:

* 0: *New container message*
* 1: *Container content message*  
* 2: *Heartbeat message*

The following subsections explain the function and structure of each type of message.

#### New Container Message
This type of message is sent to Clipper by a container at the beginning of a session. It carries container metadata (name, version, etc) that Clipper uses to register the container with the system. Beyond the required empty frame and **Message Type** field, *new container messages* contain the following strictly-ordered fields:

  * **Model Name**: The user-defined name of the model, as a string
  * **Model Version**: The **integer** model version. **This should be sent as a string**.
  * **Input Type**: The type of input that the model should accept. This must be one of the following **integer** values, sent as a **string**:
    * 0: Bytes 
    * 1: 32-bit Integers
    * 2: 32-bit Floats
    * 3: 64-bit Doubles
    * 4: Strings
    
The following is an example construction of a *new container message* in Python:

  ```py
    socket.send("", zmq.SNDMORE) # Sends an empty frame and indicates that more content will follow
    socket.send(struct.pack("<I", 0), zmq.SNDMORE) # Indicates that this is a new container message
    socket.send(<MODEL_NAME>, zmq.SNDMORE)
    socket.send(str(<MODEL_VERSION>), zmq.SNDMORE)
    socket.send(str(<MODEL_INPUT_TYPE>))
  ```
    
#### Container Content Messages
Once Clipper has registered a container, these content messages are exchanged between the container and Clipper in order to serve prediction requests. These messages contain serialized queries (from Clipper) or serialized responses (from the container). For more information on query-response serialization, see the "Serializing Prediction Requests" and "Serializing Prediction Responses" sections below. Beyond the required empty frame and **Message Type** field, *container content messages* contain the following strictly-ordered fields:

  * **Message Id**: A unique identifier, encoded as an unsigned integer, corresponding to the container content message. When handling a prediction request sent via a *container content message* from Clipper, the response *container content message* must specify the same **Message Id** as the request message. Clipper will use this identifier to correctly construct request-response pairs in order to return a query result.
  * **Message Content**: Byte content representing either a serialized prediction request (in the case of inbound messages from Clipper) or a serialized prediction response (in the case of outbound messages from the container).

The following is an example construction of a *container content message* corresponding to a prediction response in Python:

  ```py  
    socket.send("", zmq.SNDMORE) # Sends an empty frame and indicates that more content will follow
    socket.send(struct.pack("<I", 1), zmq.SNDMORE) # Indicates that this is a container content message
    socket.send(struct.pack("<I", <MESSAGE_ID>), zmq.SNDMORE)
    socket.send(<SERIALIZED_MESSAGE_CONTENT_AS_BYTES>)
  ```    
#### Heartbeat Messages
These messages are used for session initialization as well as maintenance. By sending and receiving heartbeats, containers are able to determine whether or not Clipper is still active and respond accordingly. Beyond the required empty frame and **Message Type** field, *heartbeat messages* contain the following strictly-ordered fields:

  * **Heartbeat Type (INBOUND ONLY)**: This field contains a binary, unsigned integer and is only present in **inbound** messages received from Clipper. The value meanings are as follows:
    * 1: This indicates that Clipper does not have any metadata for the recipient container. This serves as an indication that the container should send a *new container message* so that Clipper can register it.
    * 0: This indicates that the message is a simple heartbeat response from Clipper requiring no further action.
    
The following is an example construction of an **outbound** *heartbeat message* in Python:
 
  ```py      
    socket.send("", zmq.SNDMORE) # Sends an empty frame and indicates that more content will follow
    socket.send(struct.pack("<I", 2), zmq.SNDMORE) # Indicates that this is a heartbeat message
  ```
  
### Starting a new session
Now that we are familiar with the different types of RPC messages, we will see how a model container can use them to start a session with Clipper. The steps are as follows:

1. **Socket Creation**: Every Clipper session requires a distinct ZeroMQ socket. Create a [ZeroMQ Dealer socket](http://api.zeromq.org/3-2:zmq-socket) object. This socket can receive requests and send responses asynchronously.
  * Python example:
  
  ```py  
    import zmq
    context = zmq.Context();
    socket = context.socket(zmq.DEALER)
  ```
    
2. Once a ZeroMQ Dealer socket has been created, use it to connect to Clipper. In Python, this can be accomplished as follows:

  ```py  
    socket.connect(<CLIPPER_TCP_ADDRESS>, <CLIPPER_PORT>)
  ```

3. Then, send a *heartbeat message* to Clipper.

4. Continuously poll the socket for a *heartbeat message* response from Clipper. Because the container is connecting to Clipper via a fresh socket, this message should be of **heartbeat type** `1`. Python example:

  ```py  
   poller = zmq.Poller()
   poller.register(socket, zmq.POLLIN) # Register the socket for polling
   while(True):
       receivable_sockets = dict(poller.poll(5000)) # Poll the socket for new content for 5 seconds
       if socket in receivable_sockets and receivable_sockets[socket] == ZMQ.POLLIN:
           # The socket has a message to receive
           socket.recv()
           msg_type_bytes = socket.recv()
           msg_type = struct.unpack("<I", msg_type_bytes)[0]
           assert msg_type == MESSAGE_TYPE_HEARTBEAT
           heartbeat_type_bytes = socket.recv()
           heartbeat_type = struct.unpack("<I", heartbeat_type_bytes)[0]
           assert heartbeat_type == 1
           break
   ```
   
5. After receiving a heartbeat response from Clipper requesting metadata, the container should send back a **new container message**. This completes the initialization of a session.

### Maintaining a session
In order to maintain a session, the container should frequently request heartbeats from Clipper as well as poll its sockets for new *heartbeat* and *container content* messages using the following pattern:

1. Maintain a `last_activity` timestamp that will be used to infer whether or not Clipper is still alive. 

2. Poll the container socket for new messages for a predefined duration `D`. If a message is received before `D` elapses, process the message according to its type (*heartbeat* or *container content*) and set `last_activity` to the current time.

3. If `D` elapses, check the time elapsed since `last_activity`. If this exceeds the **socket activity timeout** `TO`, end the session. Otherwise, send a heartbeat message to Clipper.

4. Repeat steps 1-3. 

Python example:

  ```py  
   from datetime import datetime
   last_activity_time = datetime.now()
   while(True):
       receivable_sockets = dict(poller.poll(5000)) # Poll the socket for new content for 5 seconds
       if socket in receivable_sockets and receivable_sockets[socket] == ZMQ.POLLIN:
           # The socket has a message to receive
           socket.recv()
           msg_type_bytes = socket.recv()
           msg_type = struct.unpack("<I", msg_type_bytes)[0]
           # Process message according to type
           ...
           last_activity_time = datetime.now()
       else:
           time_since_activity = (datetime.now() - last_activity_time).seconds
           if time_since_activity >= 60:
               # Terminate the connection
               ...
           else:
               send_heartbeat_message(socket)
   ```

The current implementation uses the values `D = 5 seconds` and `TO = 30 seconds`. 

### Ending a session
If the container fails to receive a message from Clipper within the time limit specified by the **socket activity timeout**, the session should be terminated by executing the following steps:

1. Unregister the socket from any ZeroMQ pollers.

2. Close the socket.

Python example:

  ```py  
   poller.unregister(socket)
   socket.close()
  ```
The container should then attempt to start a new session.

### For additional container lifecycle references, see:
- [clipper/containers/python/rpc.py](https://github.com/ucbrise/clipper/blob/develop/containers/python/rpc.py)
- [clipper/containers/java/.../ModelContainer.java](https://github.com/ucbrise/clipper/blob/develop/containers/java/clipper-java-container/src/main/java/clipper/container/app/ModelContainer.java)

## Serialization Formats
RPC requests sent from Clipper to model containers are divided into two categories: **Prediction Requests** and **Feedback Requests**. Additionally, responses are divided into two corresponding categories: **Prediction Responses** and **Feedback Responses**. Each request type has a specific serialization format that defines the container deserialization procedure.

### Serializing Prediction Requests
1. All requests begin with a 32-bit integer header, sent as a single ZeroMQ message. The value of this integer will be 0, indicating that the request is a prediction request.

2. The second ZeroMQ message contains the size of the input header, in bytes, as a 32-bit integer. This is the size of the content of the third ZeroMQ message.

3. The third ZeroMQ message contains an input header. This is a list of 32-bit integers.
 * The input header begins with a 32-bit integer specifying the type of inputs contained in the request. This integer can assume values 0-4, as defined in point 2 of **Initializing a Connection**.
 
 * The next 32-bit integer in the input header is the number of inputs included in the serialized content.

 * The remaining values in the input header correspond to the offsets at which the deserialized output should be split.
   * For example, if the request contains three double vectors of size 500, the offsets will be `[500, 1000]`, indicating that the deserialized vector of 1500 doubles should be split into three vectors containing doubles 0-499, 500-999, and 1000-1499 respectively.
   
    * In the case of strings, the offset list is not relevant and should not be used.
    
4. The fourth ZeroMQ message contains the size of the input content, in bytes, as a 32-bit integer. This is the size of the content of the fifth ZeroMQ message.
   
5. The final ZeroMQ message contains the concatenation of all inputs, represented as a string of bytes. This string of bytes should be converted to an array of the type specified by the input header.
 * In the case of primitive inputs (types 0-3), deserialized inputs can then be obtained by splitting the typed array at the offsets specified in the input header.
   * Python example:
   
  ```py  
     raw_concatenated_content = socket.recv()
     typed_inputs = np.frombuffer(raw_concatenated_content, dtype=<PRIMITIVE_INPUT_TYPE>)
     inputs = np.split(typed_inputs, <OFFSETS_LIST>)
  ```
 
 * In the case of string inputs (type 4), all strings are sent with trailing null terminators. Therefore, deserialized inputs can be obtaining by splitting the typed array along the null terminator character, `\0`.
   * Python example:
   
  ```py
     raw_concatenated_content = socket.recv()
     # Split content based on trailing null terminators
     # Ignore the extraneous final null terminator by using a -1 slice
     inputs = np.array(raw_concatenated_content.split('\0')[:-1], dtype=np.string_)
  ```
     
### Serializing Prediction Responses
1. A response is a single ZeroMQ message that is parsed into subfields

2. The message begins with a 32-bit unsigned integer indicating the number of serialized string outputs contained in the response. Denote this quantity by `N`.

3. Next, there are 'N' ordered 32-bit unsigned integers. The `i`th integer specifies the length of the `i`th string output.

4. The remainder of the message contains the `N` string outputs, encoded in [UTF-8](https://en.wikipedia.org/wiki/UTF-8) format. 

The following is an example of **Prediction Response** serialization in Python:

  ```py
     import struct
     str1 = unicode("test1", "utf-8").encode("utf-8")
     str2 = unicode("test2", "utf-8").encode("utf-8")
     output_length_size_bytes = 4
     num_output_size_bytes = 4
     buf = bytearray(len(str1) + len(str2) + (2 * output_length_size_bytes) + num_output_size_bytes)
     memview = memoryview(buf)
     struct.pack_into("<I", buf, 0, 2) # Store the number of outputs in the buffer
     struct.pack_into("<I", buf, 4, len(str1)) # Store the length of the first output
     struct.pack_into("<I", buf, 8, len(str2)) # Store the length of the second output
     memview[12:12 + len(str1)] = str1 # Store the first output
     memview[12 + len(str1): 12 + len(str1) + len(str2)] = str2 # Store the second output
  ```

### For additional deserialization references, see [clipper/containers/python/rpc.py](https://github.com/ucbrise/clipper/blob/develop/containers/python/rpc.py)
