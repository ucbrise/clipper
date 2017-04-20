# Container Implementation Details
The following is relevant for the implementation of model containers in different languages. 
Model containers must implement functionality consistent with this information.

## Connection Lifecycle
The connection lifecycle defines the socket construction, destruction, and state-based behavior for a single Clipper session. We define a session as a sustained connection between a container and Clipper. Key points:

- Every session requires a distinct socket. When a session ends, a container must create a new socket in order to reconnect to Clipper
- Sessions are initiated and sustained by a heartbeating process. The initial detection of a two-way heartbeat between Clipper and a container marks the creation of a successful connection, and the loss of heartbeat indicates that a connection is broken.

### Socket Creation 
Every Clipper session requires a distinct ZeroMQ socket. To create a socket for connecting to Clipper, proceed as follows:

- Create a [ZeroMQ Dealer socket](http://api.zeromq.org/3-2:zmq-socket) object. This socket can receive requests and send responses asynchronously.
  * Python example:
  
    ```
    import zmq
    context = zmq.Context();
    socket = context.socket(zmq.DEALER)
    ```

### Initializing a Connection
1. Once a ZeroMQ Dealer socket has been created, use it to connect to Clipper. In Python, this can be accomplished as follows:

    ```
    socket.connect(<CLIPPER_TCP_ADDRESS>, <CLIPPER_PORT>)
    ```
2. Then, send a heartbeat message to Clipper. This message should be preceded by an [empty ZeroMQ frame](http://zguide.zeromq.org/php:chapter3#The-Simple-Reply-Envelope). This is a multi-part ZeroMQ message containing the following fields:

    
    
2. Then, send a series of ordered messages providing information about the model. This message should be preceded by an 
[empty ZeroMQ frame](http://zguide.zeromq.org/php:chapter3#The-Simple-Reply-Envelope). The following attributes should then be sent in order:
  * **Model Name**: The user-defined name of the model, as a string
  * **Model Version**: The **integer** model version. **This should be sent as a string**.
  * **Input Type**: The type of input that the model should accept. This must be one of the following **integer** values, sent as a **string**:
    * 0: Bytes 
    * 1: 32-bit Integers
    * 2: 32-bit Floats
    * 3: 64-bit Doubles
    * 4: Strings
    
  * Python example:
  
    ```
    socket.send("", zmq.SNDMORE); # Sends an empty frame and indicates that more content will follow
    socket.send(<MODEL_NAME>, zmq.SNDMORE);
    socket.send(str(<MODEL_VERSION>), zmq.SNDMORE);
    socket.send(str(<MODEL_INPUT_TYPE>))
    ```
    
3. The socket should then be continually polled for requests from Clipper.
  * Python example:
  
  ```
  while True:
    # Receive empty frame that Clipper sends before every request
    socket.recv()
    # Receive the unique id associated with the request message
    msg_id_bytes = socket.recv()
    # Receive the request header
    request_header = socket.recv()
    # Process the request based on header data
    ...
  ```

## Serialization Formats
RPC requests sent from Clipper to model containers are divided into two categories: **Prediction Requests** and **Feedback Requests**. Each request type has a specific serialization format that defines the container deserialization procedure.

### Serializing Prediction Requests
1. All requests begin with a 32-bit integer header, sent as a single ZeroMQ message. The value of this integer will be 0, indicating that the request is a prediction request.

2. The second ZeroMQ message contains the size of the input header, in bytes, as a 64-bit long. This is the size of the content of the third ZeroMQ message.

3. The third ZeroMQ message contains an input header. This is a list of 32-bit integers.
 * The input header begins with a 32-bit integer specifying the type of inputs contained in the request. This integer can assume values 0-4, as defined in point 2 of **Initializing a Connection**.
 
 * The next 32-bit integer in the input header is the number of inputs included in the serialized content.

 * The remaining values in the input header correspond to the offsets at which the deserialized output should be split.
   * For example, if the request contains three double vectors of size 500, the offsets will be `[500, 1000]`, indicating that the deserialized vector of 1500 doubles should be split into three vectors containing doubles 0-499, 500-999, and 1000-1499 respectively.
   
    * In the case of strings, the offset list is not relevant and should not be used.
    
4. The fourth ZeroMQ message contains the size of the input content, in bytes, as a 64-bit long. This is the size of the content of the fifth ZeroMQ message.
   
5. The final ZeroMQ message contains the concatenation of all inputs, represented as a string of bytes. This string of bytes should be converted to an array of the type specified by the input header.
 * In the case of primitive inputs (types 0-3), deserialized inputs can then be obtained by splitting the typed array at the offsets specified in the input header.
   * Python example:
   
     ```
     raw_concatenated_content = socket.recv()
     typed_inputs = np.frombuffer(raw_concatenated_content, dtype=<PRIMITIVE_INPUT_TYPE>)
     inputs = np.split(typed_inputs, <OFFSETS_LIST>)
     ```
 
 * In the case of string inputs (type 4), all strings are sent with trailing null terminators. Therefore, deserialized inputs can be obtaining by splitting the typed array along the null terminator character, `\0`.
   * Python example:
   
     ```
     raw_concatenated_content = socket.recv()
     # Split content based on trailing null terminators
     # Ignore the extraneous final null terminator by using a -1 slice
     inputs = np.array(raw_concatenated_content.split('\0')[:-1], dtype=np.string_)
     ```

#### For additional deserialization references, see `clipper/containers/python/rpc.py`
