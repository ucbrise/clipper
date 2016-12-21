# Container Implementation Details
The following is relevant for the implementation of model containers in different languages. 
Model containers must implement functionality consistent with this information.

## Socket Creation 
- Create a [ZeroMQ Dealer socket](http://api.zeromq.org/3-2:zmq-socket). This socket can receive requests and send responses asynchronously.
  * Python example:
  
    ```
    import zmq
    context = zmq.Context();
    socket = context.socket(zmq.DEALER)
    ```

## Initializing a Connection
- Once a ZeroMQ Dealer socket has been created, use it to connect to Clipper. In Python, this can be accomplished as follows:

    ```
    socket.connect(<CLIPPER_TCP_ADDRESS>, <CLIPPER_PORT>)
    ```
- Then, send a series of ordered messages providing information about the model. This message should be preceded by an 
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
    
- The socket should then be continually polled for requests from Clipper.
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
  


## Serialiazation Formats
