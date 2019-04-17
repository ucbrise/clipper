/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* 
 *
This example contributed by Electronic Arts, Inc.
Author: Arpit Baldeva
 */

/* 
This example demonstrates RouteGuide Server example code implemented in Async API fashion. The example below uses two threads. One thread is dedicated to the completion queue processing
of gRPC and another thread handles the completion queue tags/events as they become available. The gRPC completion queue is put on a separate thread in order to avoid blocking the main
thread of the application when no events are available in the completion queue. 

The code below implements fully generic classes for the 4 different types of rpcs found in gRPC (unary, server streaming, client streaming and bidirectional streaming). They can be used by
application to avoid writing all the state management code with the gRPC. The code has been tested against version 1.0.0 of the library. 

The 4 different implementation classes duplicate code from each other - purely as a matter of readability in this example. I implemnted a version without duplication but that made the code
considerably difficult to read (comparatively) .
*/

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <random>
#include <unordered_map>
#include <atomic>

#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>
#include <grpc++/support/async_stream.h>

#include "helper.h"
#include "route_guide.grpc.pb.h"

class ServerImpl;
// Globals to help with this example
static ServerImpl* gServerImpl;
std::string gDB;

// Globals to analyze the results and make sure we are not leaking any rpcs
static std::atomic_int32_t gUnaryRpcCounter = 0;
static std::atomic_int32_t gServerStreamingRpcCounter = 0;
static std::atomic_int32_t gClientStreamingRpcCounter = 0;
static std::atomic_int32_t gBidirectionalStreamingRpcCounter = 0;

// We add a 'TagProcessor' to the completion queue for each event. This way, each tag knows how to process itself. 
using TagProcessor = std::function<void(bool)>;
struct TagInfo
{
    TagProcessor* tagProcessor; // The function to be called to process incoming event
    bool ok; // The result of tag processing as indicated by gRPC library. Calling it 'ok' to be in sync with other gRPC examples.
};

using TagList = std::list<TagInfo>;

// As the tags become available from completion queue thread, we put them in a queue in order to process them on our application thread. 
static TagList gIncomingTags;
std::mutex gIncomingTagsMutex;

// Random sleep code in order to introduce some randomness in this example. This allows for quick stress testing. 
static void randomSleepThisThread(int lowerBoundMS, int upperBoundMS)
{
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<> dist{ lowerBoundMS, upperBoundMS };
    std::this_thread::sleep_for(std::chrono::milliseconds{ dist(generator) });
}

static void randomSleepThisThread()
{
    randomSleepThisThread(10, 100);
}


float ConvertToRadians(float num) 
{
  return num * 3.1415926 /180;
}

float GetDistance(const routeguide::Point& start, const routeguide::Point& end) 
{
  const float kCoordFactor = 10000000.0;
  float lat_1 = start.latitude() / kCoordFactor;
  float lat_2 = end.latitude() / kCoordFactor;
  float lon_1 = start.longitude() / kCoordFactor;
  float lon_2 = end.longitude() / kCoordFactor;
  float lat_rad_1 = ConvertToRadians(lat_1);
  float lat_rad_2 = ConvertToRadians(lat_2);
  float delta_lat_rad = ConvertToRadians(lat_2-lat_1);
  float delta_lon_rad = ConvertToRadians(lon_2-lon_1);

  float a = pow(sin(delta_lat_rad/2), 2) + cos(lat_rad_1) * cos(lat_rad_2) *
            pow(sin(delta_lon_rad/2), 2);
  float c = 2 * atan2(sqrt(a), sqrt(1-a));
  int R = 6371000; // metres

  return R * c;
}

std::string GetFeatureName(const routeguide::Point& point, const std::vector<routeguide::Feature>& feature_list) 
{
  for (const routeguide::Feature& f : feature_list) 
  {
    if (f.location().latitude() == point.latitude() &&
        f.location().longitude() == point.longitude()) 
    {
      return f.name();
    }
  }
  return "";
}

// A base class for various rpc types. With gRPC, it is necessary to keep track of pending async operations.
// Only 1 async operation can be pending at a time with an exception that both async read and write can be pending at the same time.
class RpcJob
{
public:
    enum AsyncOpType
    {
        ASYNC_OP_TYPE_INVALID,
        ASYNC_OP_TYPE_QUEUED_REQUEST,
        ASYNC_OP_TYPE_READ,
        ASYNC_OP_TYPE_WRITE,
        ASYNC_OP_TYPE_FINISH
    };

    RpcJob()
        : mAsyncOpCounter(0)
        , mAsyncReadInProgress(false)
        , mAsyncWriteInProgress(false)
        , mOnDoneCalled(false)
    {

    }

    virtual ~RpcJob() {};

    void AsyncOpStarted(AsyncOpType opType)
    {
        ++mAsyncOpCounter;

        switch (opType)
        {
        case ASYNC_OP_TYPE_READ:
            mAsyncReadInProgress = true;
            break;
        case ASYNC_OP_TYPE_WRITE:
            mAsyncWriteInProgress = true;
        default: //Don't care about other ops
            break;
        }
    }

    // returns true if the rpc processing should keep going. false otherwise.
    bool AsyncOpFinished(AsyncOpType opType)
    {
        --mAsyncOpCounter;

        switch (opType)
        {
        case ASYNC_OP_TYPE_READ:
            mAsyncReadInProgress = false;
            break;
        case ASYNC_OP_TYPE_WRITE:
            mAsyncWriteInProgress = false;
        default: //Don't care about other ops
            break;
        }

        // No async operations are pending and gRPC library notified as earlier that it is done with the rpc.
        // Finish the rpc. 
        if (mAsyncOpCounter == 0 && mOnDoneCalled)
        {
            Done();
            return false;
        }

        return true;
    }

    bool AsyncOpInProgress() const
    {
        return mAsyncOpCounter != 0;
    }

    bool AsyncReadInProgress() const
    {
        return mAsyncReadInProgress;
    }

    bool AsyncWriteInProgress() const
    {
        return mAsyncWriteInProgress;
    }

    // Tag processor for the 'done' event of this rpc from gRPC library
    void OnDone(bool /*ok*/)
    {
        mOnDoneCalled = true;
        if (mAsyncOpCounter == 0)
            Done();
    }

    // Each different rpc type need to implement the specialization of action when this rpc is done.
    virtual void Done() = 0;
private:
    int32_t mAsyncOpCounter;
    bool mAsyncReadInProgress;
    bool mAsyncWriteInProgress;

    // In case of an abrupt rpc ending (for example, client process exit), gRPC calls OnDone prematurely even while an async operation is in progress
    // and would be notified later. An example sequence would be
    // 1. The client issues an rpc request. 
    // 2. The server handles the rpc and calls Finish with response. At this point, ServerContext::IsCancelled is NOT true.
    // 3. The client process abruptly exits. 
    // 4. The completion queue dispatches an OnDone tag followed by the OnFinish tag. If the application cleans up the state in OnDone, OnFinish invocation would result in undefined behavior. 
    // This actually feels like a pretty odd behavior of the gRPC library (it is most likely a result of our multi-threaded usage) so we account for that by keeping track of whether the OnDone was called earlier. 
    // As far as the application is considered, the rpc is only 'done' when no asyn Ops are pending. 
    bool mOnDoneCalled;
};

// The application code communicates with our utility classes using these handlers. 
template<typename ServiceType, typename RequestType, typename ResponseType>
struct RpcJobHandlers
{
public:
    // typedefs. See the comments below. 
    using ProcessRequestHandler = std::function<void(ServiceType*, RpcJob*, const RequestType*)>;
    using CreateRpcJobHandler = std::function<void()>;
    using RpcJobDoneHandler = std::function<void(ServiceType*, RpcJob*, bool)>;

    using SendResponseHandler = std::function<bool(const ResponseType*)>; // GRPC_TODO - change to a unique_ptr instead to avoid internal copying.
    using RpcJobContextHandler = std::function<void(ServiceType*, RpcJob*, grpc::ServerContext*, SendResponseHandler)>;

    // Job to Application code handlers/callbacks
    ProcessRequestHandler processRequestHandler; // RpcJob calls this to inform the application of a new request to be processed. 
    CreateRpcJobHandler createRpcJobHandler; // RpcJob calls this to inform the application to create a new RpcJob of this type.
    RpcJobDoneHandler rpcJobDoneHandler; // RpcJob calls this to inform the application that this job is done now. 

    // Application code to job
    RpcJobContextHandler rpcJobContextHandler; // RpcJob calls this to inform the application of the entities it can use to respond to the rpc request.
};

// Each rpc type specializes RpcJobHandlers by deriving from it as each of them have a different responder to talk back to gRPC library.
template<typename ServiceType, typename RequestType, typename ResponseType>
struct UnaryRpcJobHandlers : public RpcJobHandlers<ServiceType, RequestType, ResponseType>
{
public:
    using GRPCResponder = grpc::ServerAsyncResponseWriter<ResponseType>;
    using QueueRequestHandler = std::function<void(ServiceType*, grpc::ServerContext*, RequestType*, GRPCResponder*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void *)>;

    // Job to Application code handlers/callbacks
    QueueRequestHandler queueRequestHandler; // RpcJob calls this to inform the application to queue up a request for enabling rpc handling.
};

template<typename ServiceType, typename RequestType, typename ResponseType>
struct ServerStreamingRpcJobHandlers : public RpcJobHandlers<ServiceType, RequestType, ResponseType>
{
public:
    using GRPCResponder = grpc::ServerAsyncWriter<ResponseType>;
    using QueueRequestHandler = std::function<void(ServiceType*, grpc::ServerContext*, RequestType*, GRPCResponder*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void *)>;

    // Job to Application code handlers/callbacks
    QueueRequestHandler queueRequestHandler; // RpcJob calls this to inform the application to queue up a request for enabling rpc handling.
};

template<typename ServiceType, typename RequestType, typename ResponseType>
struct ClientStreamingRpcJobHandlers : public RpcJobHandlers<ServiceType, RequestType, ResponseType>
{
public:
    using GRPCResponder = grpc::ServerAsyncReader<ResponseType, RequestType>;
    using QueueRequestHandler = std::function<void(ServiceType*, grpc::ServerContext*, GRPCResponder*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void *)>;

    // Job to Application code handlers/callbacks
    QueueRequestHandler queueRequestHandler; // RpcJob calls this to inform the application to queue up a request for enabling rpc handling.
};

template<typename ServiceType, typename RequestType, typename ResponseType>
struct BidirectionalStreamingRpcJobHandlers : public RpcJobHandlers<ServiceType, RequestType, ResponseType>
{
public:
    using GRPCResponder = grpc::ServerAsyncReaderWriter<ResponseType, RequestType>;
    using QueueRequestHandler = std::function<void(ServiceType*, grpc::ServerContext*, GRPCResponder*, grpc::CompletionQueue*, grpc::ServerCompletionQueue*, void *)>;

    // Job to Application code handlers/callbacks
    QueueRequestHandler queueRequestHandler; // RpcJob calls this to inform the application to queue up a request for enabling rpc handling.
};


/*
We implement UnaryRpcJob, ServerStreamingRpcJob, ClientStreamingRpcJob and BidirectionalStreamingRpcJob. The application deals with these classes.

As a convention, we always send grpc::Status::OK and add any error info in the google.rpc.Status member of the ResponseType field. In streaming scenarios, this allows us to indicate error
in a request to a client without completion of the rpc (and allow for more requests on same rpc). We do, however, allow server side cancellation of the rpc.
*/

template<typename ServiceType, typename RequestType, typename ResponseType>
class UnaryRpcJob : public RpcJob
{
    using ThisRpcTypeJobHandlers = UnaryRpcJobHandlers<ServiceType, RequestType, ResponseType>;

public:
    UnaryRpcJob(ServiceType* service, grpc::ServerCompletionQueue* cq, ThisRpcTypeJobHandlers jobHandlers)
        : mService(service)
        , mCQ(cq)
        , mResponder(&mServerContext)
        , mHandlers(jobHandlers)
    {
        ++gUnaryRpcCounter;

        // create TagProcessors that we'll use to interact with gRPC CompletionQueue
        mOnRead = std::bind(&UnaryRpcJob::OnRead, this, std::placeholders::_1);
        mOnFinish = std::bind(&UnaryRpcJob::OnFinish, this, std::placeholders::_1);
        mOnDone = std::bind(&RpcJob::OnDone, this, std::placeholders::_1);

        // set up the completion queue to inform us when gRPC is done with this rpc.
        mServerContext.AsyncNotifyWhenDone(&mOnDone);

        // inform the application of the entities it can use to respond to the rpc
        mSendResponse = std::bind(&UnaryRpcJob::SendResponse, this, std::placeholders::_1);
        jobHandlers.rpcJobContextHandler(mService, this, &mServerContext, mSendResponse);

        // finally, issue the async request needed by gRPC to start handling this rpc.
        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST);
        mHandlers.queueRequestHandler(mService, &mServerContext, &mRequest, &mResponder, mCQ, mCQ, &mOnRead);
    }

private:

    bool SendResponse(const ResponseType* response)
    {
        // We always expect a valid response for Unary rpc. If no response is available, use ServerContext::TryCancel.
        GPR_ASSERT(response);
        if (response == nullptr)
            return false;

        mResponse = *response;

        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_FINISH);
        mResponder.Finish(mResponse, grpc::Status::OK, &mOnFinish);

        return true;
    }

    void OnRead(bool ok)
    {
        // A request has come on the service which can now be handled. Create a new rpc of this type to allow the server to handle next request.
        mHandlers.createRpcJobHandler();

        if (AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST))
        {
            if (ok)
            {
                // We have a request that can be responded to now. So process it. 
                mHandlers.processRequestHandler(mService, this, &mRequest);
            }
            else
            {
                GPR_ASSERT(ok);
            }
        }
    }

    void OnFinish(bool ok)
    {
        AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_FINISH);
    }

    void Done() override
    {
        mHandlers.rpcJobDoneHandler(mService, this, mServerContext.IsCancelled());

        --gUnaryRpcCounter;
        gpr_log(GPR_DEBUG, "Pending Unary Rpcs Count = %d", gUnaryRpcCounter);
    }

private:

    ServiceType* mService;
    grpc::ServerCompletionQueue* mCQ;
    typename ThisRpcTypeJobHandlers::GRPCResponder mResponder;
    grpc::ServerContext mServerContext;

    RequestType mRequest;
    ResponseType mResponse;

    ThisRpcTypeJobHandlers mHandlers;

    typename ThisRpcTypeJobHandlers::SendResponseHandler mSendResponse;

    TagProcessor mOnRead;
    TagProcessor mOnFinish;
    TagProcessor mOnDone;
};



template<typename ServiceType, typename RequestType, typename ResponseType>
class ServerStreamingRpcJob : public RpcJob
{
    using ThisRpcTypeJobHandlers = ServerStreamingRpcJobHandlers<ServiceType, RequestType, ResponseType>;

public:
    ServerStreamingRpcJob(ServiceType* service, grpc::ServerCompletionQueue* cq, ThisRpcTypeJobHandlers jobHandlers)
        : mService(service)
        , mCQ(cq)
        , mResponder(&mServerContext)
        , mHandlers(jobHandlers)
        , mServerStreamingDone(false)
    {
        ++gServerStreamingRpcCounter;

        // create TagProcessors that we'll use to interact with gRPC CompletionQueue
        mOnRead = std::bind(&ServerStreamingRpcJob::OnRead, this, std::placeholders::_1);
        mOnWrite = std::bind(&ServerStreamingRpcJob::OnWrite, this, std::placeholders::_1);
        mOnFinish = std::bind(&ServerStreamingRpcJob::OnFinish, this, std::placeholders::_1);
        mOnDone = std::bind(&RpcJob::OnDone, this, std::placeholders::_1);

        // set up the completion queue to inform us when gRPC is done with this rpc.
        mServerContext.AsyncNotifyWhenDone(&mOnDone);

        //inform the application of the entities it can use to respond to the rpc
        mSendResponse = std::bind(&ServerStreamingRpcJob::SendResponse, this, std::placeholders::_1);
        jobHandlers.rpcJobContextHandler(mService, this, &mServerContext, mSendResponse);

        // finally, issue the async request needed by gRPC to start handling this rpc.
        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST);
        mHandlers.queueRequestHandler(mService, &mServerContext, &mRequest, &mResponder, mCQ, mCQ, &mOnRead);
    }

private:

    // gRPC can only do one async write at a time but that is very inconvenient from the application point of view.
    // So we buffer the response below in a queue if gRPC lib is not ready for it. 
    // The application can send a null response in order to indicate the completion of server side streaming. 
    bool SendResponse(const ResponseType* response)
    {
        if (response != nullptr)
        {
            mResponseQueue.push_back(*response);

            if (!AsyncWriteInProgress())
            {
                doSendResponse();
            }
        }
        else
        {
            mServerStreamingDone = true;

            if (!AsyncWriteInProgress())
            {
                doFinish();
            }
        }

        return true;
    }

    void doSendResponse()
    {
        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_WRITE);
        mResponder.Write(mResponseQueue.front(), &mOnWrite);
    }

    void doFinish()
    {
        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_FINISH);
        mResponder.Finish(grpc::Status::OK, &mOnFinish);
    }

    void OnRead(bool ok)
    {
        mHandlers.createRpcJobHandler();

        if (AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST))
        {
            if (ok)
            {
                mHandlers.processRequestHandler(mService, this, &mRequest);
            }
        }
    }

    void OnWrite(bool ok)
    {
        if (AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_WRITE))
        {
            // Get rid of the message that just finished.
            mResponseQueue.pop_front();

            if (ok)
            {
                if (!mResponseQueue.empty()) // If we have more messages waiting to be sent, send them.
                {
                    doSendResponse();
                }
                else if (mServerStreamingDone) // Previous write completed and we did not have any pending write. If the application has finished streaming responses, finish the rpc processing.
                {
                    doFinish();
                }
            }
        }
    }

    void OnFinish(bool ok)
    {
        AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_FINISH);
    }

    void Done() override
    {
        mHandlers.rpcJobDoneHandler(mService, this, mServerContext.IsCancelled());

        --gServerStreamingRpcCounter;
        gpr_log(GPR_DEBUG, "Pending Server Streaming Rpcs Count = %d", gServerStreamingRpcCounter);
    }

private:

    ServiceType* mService;
    grpc::ServerCompletionQueue* mCQ;
    typename ThisRpcTypeJobHandlers::GRPCResponder mResponder;
    grpc::ServerContext mServerContext;

    RequestType mRequest;
    
    ThisRpcTypeJobHandlers mHandlers;

    typename ThisRpcTypeJobHandlers::SendResponseHandler mSendResponse;

    TagProcessor mOnRead;
    TagProcessor mOnWrite;
    TagProcessor mOnFinish;
    TagProcessor mOnDone;

    std::list<ResponseType> mResponseQueue;
    bool mServerStreamingDone;
};


template<typename ServiceType, typename RequestType, typename ResponseType>
class ClientStreamingRpcJob : public RpcJob
{
    using ThisRpcTypeJobHandlers = ClientStreamingRpcJobHandlers<ServiceType, RequestType, ResponseType>;

public:
    ClientStreamingRpcJob(ServiceType* service, grpc::ServerCompletionQueue* cq, ThisRpcTypeJobHandlers jobHandlers)
        : mService(service)
        , mCQ(cq)
        , mResponder(&mServerContext)
        , mHandlers(jobHandlers)
        , mClientStreamingDone(false)
    {
        ++gClientStreamingRpcCounter;

        // create TagProcessors that we'll use to interact with gRPC CompletionQueue
        mOnInit = std::bind(&ClientStreamingRpcJob::OnInit, this, std::placeholders::_1);
        mOnRead = std::bind(&ClientStreamingRpcJob::OnRead, this, std::placeholders::_1);
        mOnFinish = std::bind(&ClientStreamingRpcJob::OnFinish, this, std::placeholders::_1);
        mOnDone = std::bind(&RpcJob::OnDone, this, std::placeholders::_1);

        // set up the completion queue to inform us when gRPC is done with this rpc.
        mServerContext.AsyncNotifyWhenDone(&mOnDone);

        //inform the application of the entities it can use to respond to the rpc
        mSendResponse = std::bind(&ClientStreamingRpcJob::SendResponse, this, std::placeholders::_1);
        jobHandlers.rpcJobContextHandler(mService, this, &mServerContext, mSendResponse);

        // finally, issue the async request needed by gRPC to start handling this rpc.
        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST);
        mHandlers.queueRequestHandler(mService, &mServerContext, &mResponder, mCQ, mCQ, &mOnInit);
    }

private:

    bool SendResponse(const ResponseType* response)
    {
        // We always expect a valid response for client streaming rpc. If no response is available, use ServerContext::TryCancel.
        GPR_ASSERT(response);
        if (response == nullptr)
            return false;

        if (!mClientStreamingDone)
        {
            // It does not make sense to send a response before client has streamed all the requests. Supporting this behavior could lead to writing error-prone
            // code so it is specifically disallowed. If you need to error out before client can stream all the requests, use ServerContext::TryCancel.
            GPR_ASSERT(false);
            return false;
        }

        mResponse = *response;

        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_FINISH);
        mResponder.Finish(mResponse, grpc::Status::OK, &mOnFinish);

        return true;
    }

    void OnInit(bool ok)
    {
        mHandlers.createRpcJobHandler();

        if (AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST))
        {
            if (ok)
            {
                AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_READ);
                mResponder.Read(&mRequest, &mOnRead);
            }
        }
    }

    void OnRead(bool ok)
    {
        if (AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_READ))
        {
            if (ok)
            {
                // inform application that a new request has come in
                mHandlers.processRequestHandler(mService, this, &mRequest);

                // queue up another read operation for this rpc
                AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_READ);
                mResponder.Read(&mRequest, &mOnRead);
            }
            else
            {
                mClientStreamingDone = true;
                mHandlers.processRequestHandler(mService, this, nullptr);
            }
        }
    }

    void OnFinish(bool ok)
    {
        AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_FINISH);
    }

    void Done() override
    {
        mHandlers.rpcJobDoneHandler(mService, this, mServerContext.IsCancelled());

        --gClientStreamingRpcCounter;
        gpr_log(GPR_DEBUG, "Pending Client Streaming Rpcs Count = %d", gClientStreamingRpcCounter);
    }

private:

    ServiceType* mService;
    grpc::ServerCompletionQueue* mCQ;
    typename ThisRpcTypeJobHandlers::GRPCResponder mResponder;
    grpc::ServerContext mServerContext;

    RequestType mRequest;
    ResponseType mResponse;

    ThisRpcTypeJobHandlers mHandlers;

    typename ThisRpcTypeJobHandlers::SendResponseHandler mSendResponse;

    TagProcessor mOnInit;
    TagProcessor mOnRead;
    TagProcessor mOnFinish;
    TagProcessor mOnDone;

    bool mClientStreamingDone;
};



template<typename ServiceType, typename RequestType, typename ResponseType>
class BidirectionalStreamingRpcJob : public RpcJob
{
    using ThisRpcTypeJobHandlers = BidirectionalStreamingRpcJobHandlers<ServiceType, RequestType, ResponseType>;

public:
    BidirectionalStreamingRpcJob(ServiceType* service, grpc::ServerCompletionQueue* cq, ThisRpcTypeJobHandlers jobHandlers)
        : mService(service)
        , mCQ(cq)
        , mResponder(&mServerContext)
        , mHandlers(jobHandlers)
        , mServerStreamingDone(false)
        , mClientStreamingDone(false)
    {
        ++gBidirectionalStreamingRpcCounter;

        // create TagProcessors that we'll use to interact with gRPC CompletionQueue
        mOnInit = std::bind(&BidirectionalStreamingRpcJob::OnInit, this, std::placeholders::_1);
        mOnRead = std::bind(&BidirectionalStreamingRpcJob::OnRead, this, std::placeholders::_1);
        mOnWrite = std::bind(&BidirectionalStreamingRpcJob::OnWrite, this, std::placeholders::_1);
        mOnFinish = std::bind(&BidirectionalStreamingRpcJob::OnFinish, this, std::placeholders::_1);
        mOnDone = std::bind(&RpcJob::OnDone, this, std::placeholders::_1);

        // set up the completion queue to inform us when gRPC is done with this rpc.
        mServerContext.AsyncNotifyWhenDone(&mOnDone);

        //inform the application of the entities it can use to respond to the rpc
        mSendResponse = std::bind(&BidirectionalStreamingRpcJob::SendResponse, this, std::placeholders::_1);
        jobHandlers.rpcJobContextHandler(mService, this, &mServerContext, mSendResponse);

        // finally, issue the async request needed by gRPC to start handling this rpc.
        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST);
        mHandlers.queueRequestHandler(mService, &mServerContext, &mResponder, mCQ, mCQ, &mOnInit);
    }

private:

    bool SendResponse(const ResponseType* response)
    {
        if (response == nullptr && !mClientStreamingDone)
        {
            // Wait for client to finish the all the requests. If you want to cancel, use ServerContext::TryCancel. 
            GPR_ASSERT(false);
            return false;
        }

        if (response != nullptr)
        {
            mResponseQueue.push_back(*response); // We need to make a copy of the response because we need to maintain it until we get a completion notification. 

            if (!AsyncWriteInProgress())
            {
                doSendResponse();
            }
        }
        else
        {
            mServerStreamingDone = true;

            if (!AsyncWriteInProgress()) // Kick the async op if our state machine is not going to be kicked from the completion queue
            {
                doFinish();
            }
        }

        return true;
    }

    void OnInit(bool ok)
    {
        mHandlers.createRpcJobHandler();

        if (AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_QUEUED_REQUEST))
        {
            if (ok)
            {
                AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_READ);
                mResponder.Read(&mRequest, &mOnRead);
            }
        }
    }

    void OnRead(bool ok)
    {
        if (AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_READ))
        {
            if (ok)
            {
                // inform application that a new request has come in
                mHandlers.processRequestHandler(mService, this, &mRequest);

                // queue up another read operation for this rpc
                AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_READ);
                mResponder.Read(&mRequest, &mOnRead);
            }
            else
            {
                mClientStreamingDone = true;
                mHandlers.processRequestHandler(mService, this, nullptr);
            }
        }
    }

    void OnWrite(bool ok)
    {
        if (AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_WRITE))
        {
            // Get rid of the message that just finished. 
            mResponseQueue.pop_front();

            if (ok)
            {
                if (!mResponseQueue.empty()) // If we have more messages waiting to be sent, send them.
                {
                    doSendResponse();
                }
                else if (mServerStreamingDone) // Previous write completed and we did not have any pending write. If the application indicated a done operation, finish the rpc processing.
                {
                    doFinish();
                }
            }
        }
    }

    void OnFinish(bool ok)
    {
        AsyncOpFinished(RpcJob::ASYNC_OP_TYPE_FINISH);
    }

    void Done() override
    {
        mHandlers.rpcJobDoneHandler(mService, this, mServerContext.IsCancelled());

        --gBidirectionalStreamingRpcCounter;
        gpr_log(GPR_DEBUG, "Pending Bidirectional Streaming Rpcs Count = %d", gBidirectionalStreamingRpcCounter);
    }

    void doSendResponse()
    {
        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_WRITE);
        mResponder.Write(mResponseQueue.front(), &mOnWrite);
    }

    void doFinish()
    {
        AsyncOpStarted(RpcJob::ASYNC_OP_TYPE_FINISH);
        mResponder.Finish(grpc::Status::OK, &mOnFinish);
    }

private:

    ServiceType* mService;
    grpc::ServerCompletionQueue* mCQ;
    typename ThisRpcTypeJobHandlers::GRPCResponder mResponder;
    grpc::ServerContext mServerContext;

    RequestType mRequest;

    ThisRpcTypeJobHandlers mHandlers;

    typename ThisRpcTypeJobHandlers::SendResponseHandler mSendResponse;

    TagProcessor mOnInit;
    TagProcessor mOnRead;
    TagProcessor mOnWrite;
    TagProcessor mOnFinish;
    TagProcessor mOnDone;


    std::list<ResponseType> mResponseQueue;
    bool mServerStreamingDone;
    bool mClientStreamingDone;
};

class ServerImpl final 
{
public:
    ServerImpl()
    {
        routeguide::ParseDb(gDB, &mFeatureList);
    }

    ~ServerImpl() 
    {
        mServer->Shutdown();
        // Always shutdown the completion queue after the server.
        mCQ->Shutdown();
    }

    // There is no shutdown handling in this code.
    void Run() 
    {
        std::string server_address("0.0.0.0:50051");

        grpc::ServerBuilder builder;
        // Listen on the given address without any authentication mechanism.
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        // Register "service_" as the instance through which we'll communicate with
        // clients. In this case it corresponds to an *asynchronous* service.
        builder.RegisterService(&mRouteGuideService);
        // Get hold of the completion queue used for the asynchronous communication
        // with the gRPC runtime.
        mCQ = builder.AddCompletionQueue();
        // Finally assemble the server.
        mServer = builder.BuildAndStart();
        std::cout << "Server listening on " << server_address << std::endl;

        // Proceed to the server's main loop.
        HandleRpcs();
    }

private:
    // Handlers for various rpcs. An application could do custom code generation for creating these (except the actual processing logic). 
    void createGetFeatureRpc()
    {
        UnaryRpcJobHandlers<routeguide::RouteGuide::AsyncService, routeguide::Point, routeguide::Feature> jobHandlers;
        jobHandlers.rpcJobContextHandler = &GetFeatureContextSetterImpl;
        jobHandlers.rpcJobDoneHandler = &GetFeatureDone;
        jobHandlers.createRpcJobHandler = std::bind(&ServerImpl::createGetFeatureRpc, this);
        jobHandlers.queueRequestHandler = &routeguide::RouteGuide::AsyncService::RequestGetFeature;
        jobHandlers.processRequestHandler = &GetFeatureProcessor;

        new UnaryRpcJob<routeguide::RouteGuide::AsyncService, routeguide::Point, routeguide::Feature>(&mRouteGuideService, mCQ.get(), jobHandlers);
    }
    
    struct GetFeatureResponder
    {
        std::function<bool(routeguide::Feature*)> sendFunc;
        grpc::ServerContext* serverContext;
    };

    std::unordered_map<RpcJob*, GetFeatureResponder> mGetFeatureResponders;
    static void GetFeatureContextSetterImpl(routeguide::RouteGuide::AsyncService* service, RpcJob* job, grpc::ServerContext* serverContext, std::function<bool(routeguide::Feature*)> sendResponse)
    {
        GetFeatureResponder responder;
        responder.sendFunc = sendResponse;
        responder.serverContext = serverContext;

        gServerImpl->mGetFeatureResponders[job] = responder;
    }

    static void GetFeatureProcessor(routeguide::RouteGuide::AsyncService* service, RpcJob* job, const routeguide::Point* point)
    {
        routeguide::Feature feature;
        feature.set_name(GetFeatureName(*point, gServerImpl->mFeatureList));
        feature.mutable_location()->CopyFrom(*point);
        
        randomSleepThisThread();
        gServerImpl->mGetFeatureResponders[job].sendFunc(&feature);
    }

    static void GetFeatureDone(routeguide::RouteGuide::AsyncService* service, RpcJob* job, bool rpcCancelled)
    {
        gServerImpl->mGetFeatureResponders.erase(job);
        delete job;
    }

    void createListFeaturesRpc()
    {
        ServerStreamingRpcJobHandlers<routeguide::RouteGuide::AsyncService, routeguide::Rectangle, routeguide::Feature> jobHandlers;
        jobHandlers.rpcJobContextHandler = &ListFeaturesContextSetterImpl;
        jobHandlers.rpcJobDoneHandler = &ListFeaturesDone;
        jobHandlers.createRpcJobHandler = std::bind(&ServerImpl::createListFeaturesRpc, this);
        jobHandlers.queueRequestHandler = &routeguide::RouteGuide::AsyncService::RequestListFeatures;
        jobHandlers.processRequestHandler = &ListFeaturesProcessor;

        new ServerStreamingRpcJob<routeguide::RouteGuide::AsyncService, routeguide::Rectangle, routeguide::Feature>(&mRouteGuideService, mCQ.get(), jobHandlers);
    }

    struct ListFeaturesResponder
    {
        std::function<bool(routeguide::Feature*)> sendFunc;
        grpc::ServerContext* serverContext;
    };

    std::unordered_map<RpcJob*, ListFeaturesResponder> mListFeaturesResponders;
    static void ListFeaturesContextSetterImpl(routeguide::RouteGuide::AsyncService* service, RpcJob* job, grpc::ServerContext* serverContext, std::function<bool(routeguide::Feature*)> sendResponse)
    {
        ListFeaturesResponder responder;
        responder.sendFunc = sendResponse;
        responder.serverContext = serverContext;

        gServerImpl->mListFeaturesResponders[job] = responder;
    }

    static void ListFeaturesProcessor(routeguide::RouteGuide::AsyncService* service, RpcJob* job, const routeguide::Rectangle* rectangle)
    {
        auto lo = rectangle->lo();
        auto hi = rectangle->hi();
        long left = (std::min)(lo.longitude(), hi.longitude());
        long right = (std::max)(lo.longitude(), hi.longitude());
        long top = (std::max)(lo.latitude(), hi.latitude());
        long bottom = (std::min)(lo.latitude(), hi.latitude());
        for (auto f : gServerImpl->mFeatureList) {
            if (f.location().longitude() >= left &&
                f.location().longitude() <= right &&
                f.location().latitude() >= bottom &&
                f.location().latitude() <= top) {
                gServerImpl->mListFeaturesResponders[job].sendFunc(&f);
                randomSleepThisThread();
            }
        }
        gServerImpl->mListFeaturesResponders[job].sendFunc(nullptr);
    }

    static void ListFeaturesDone(routeguide::RouteGuide::AsyncService* service, RpcJob* job, bool rpcCancelled)
    {
        gServerImpl->mListFeaturesResponders.erase(job);
        delete job;
    }

    void createRecordRouteRpc()
    {
        ClientStreamingRpcJobHandlers<routeguide::RouteGuide::AsyncService, routeguide::Point, routeguide::RouteSummary> jobHandlers;
        jobHandlers.rpcJobContextHandler = &RecordRouteContextSetterImpl;
        jobHandlers.rpcJobDoneHandler = &RecordRouteDone;
        jobHandlers.createRpcJobHandler = std::bind(&ServerImpl::createRecordRouteRpc, this);
        jobHandlers.queueRequestHandler = &routeguide::RouteGuide::AsyncService::RequestRecordRoute;
        jobHandlers.processRequestHandler = &RecordRouteProcessor;

        new ClientStreamingRpcJob<routeguide::RouteGuide::AsyncService, routeguide::Point, routeguide::RouteSummary>(&mRouteGuideService, mCQ.get(), jobHandlers);
    }

    struct RecordRouteResponder
    {
        std::function<bool(routeguide::RouteSummary*)> sendFunc;
        grpc::ServerContext* serverContext;
    };

    std::unordered_map<RpcJob*, RecordRouteResponder> mRecordRouteResponders;
    static void RecordRouteContextSetterImpl(routeguide::RouteGuide::AsyncService* service, RpcJob* job, grpc::ServerContext* serverContext, std::function<bool(routeguide::RouteSummary*)> sendResponse)
    {
        RecordRouteResponder responder;
        responder.sendFunc = sendResponse;
        responder.serverContext = serverContext;

        gServerImpl->mRecordRouteResponders[job] = responder;
    }

    struct RecordRouteState
    {
        int pointCount;
        int featureCount;
        float distance;
        routeguide::Point previous;
        std::chrono::system_clock::time_point startTime;
        RecordRouteState()
            : pointCount(0)
            , featureCount(0)
            , distance(0.0f)
        {

        }
    };

    std::unordered_map<RpcJob*, RecordRouteState> mRecordRouteMap;
    static void RecordRouteProcessor(routeguide::RouteGuide::AsyncService* service, RpcJob* job, const routeguide::Point* point)
    {
        RecordRouteState& state = gServerImpl->mRecordRouteMap[job];

        if (point)
        {
            if (state.pointCount == 0)
                state.startTime = std::chrono::system_clock::now();

            state.pointCount++;
            if (!GetFeatureName(*point, gServerImpl->mFeatureList).empty()) {
                state.featureCount++;
            }
            if (state.pointCount != 1) {
                state.distance += GetDistance(state.previous, *point);
            }
            state.previous = *point;

            randomSleepThisThread();
        }
        else
        {
            std::chrono::system_clock::time_point endTime = std::chrono::system_clock::now();
            
            routeguide::RouteSummary summary;
            summary.set_point_count(state.pointCount);
            summary.set_feature_count(state.featureCount);
            summary.set_distance(static_cast<long>(state.distance));
            auto secs = std::chrono::duration_cast<std::chrono::seconds>(endTime - state.startTime);
            summary.set_elapsed_time(secs.count());
            gServerImpl->mRecordRouteResponders[job].sendFunc(&summary);

            gServerImpl->mRecordRouteMap.erase(job);

            randomSleepThisThread();
        }
    }

    static void RecordRouteDone(routeguide::RouteGuide::AsyncService* service, RpcJob* job, bool rpcCancelled)
    {
        gServerImpl->mRecordRouteResponders.erase(job);
        delete job;
    }

    void createRouteChatRpc()
    {
        BidirectionalStreamingRpcJobHandlers<routeguide::RouteGuide::AsyncService, routeguide::RouteNote, routeguide::RouteNote> jobHandlers;
        jobHandlers.rpcJobContextHandler = &RouteChatContextSetterImpl;
        jobHandlers.rpcJobDoneHandler = &RouteChatDone;
        jobHandlers.createRpcJobHandler = std::bind(&ServerImpl::createRouteChatRpc, this);
        jobHandlers.queueRequestHandler = &routeguide::RouteGuide::AsyncService::RequestRouteChat;
        jobHandlers.processRequestHandler = &RouteChatProcessor;

        new BidirectionalStreamingRpcJob<routeguide::RouteGuide::AsyncService, routeguide::RouteNote, routeguide::RouteNote>(&mRouteGuideService, mCQ.get(), jobHandlers);
    }

    struct RouteChatResponder
    {
        std::function<bool(routeguide::RouteNote*)> sendFunc;
        grpc::ServerContext* serverContext;
    };

    std::unordered_map<RpcJob*, RouteChatResponder> mRouteChatResponders;
    static void RouteChatContextSetterImpl(routeguide::RouteGuide::AsyncService* service, RpcJob* job, grpc::ServerContext* serverContext, std::function<bool(routeguide::RouteNote*)> sendResponse)
    {
        RouteChatResponder responder;
        responder.sendFunc = sendResponse;
        responder.serverContext = serverContext;

        gServerImpl->mRouteChatResponders[job] = responder;
    }
    
    static void RouteChatProcessor(routeguide::RouteGuide::AsyncService* service, RpcJob* job, const routeguide::RouteNote* note)
    {
        //Simply echo the note back.
        if (note)
        {
            routeguide::RouteNote responseNote(*note);
            gServerImpl->mRouteChatResponders[job].sendFunc(&responseNote);
            randomSleepThisThread();
        }
        else
        {
            gServerImpl->mRouteChatResponders[job].sendFunc(nullptr);
            randomSleepThisThread();
        }
    }
   
    static void RouteChatDone(routeguide::RouteGuide::AsyncService* service, RpcJob* job, bool rpcCancelled)
    {
        gServerImpl->mRouteChatResponders.erase(job);
        delete job;
    }

    void HandleRpcs() 
    {
        createGetFeatureRpc();
        createListFeaturesRpc();
        createRecordRouteRpc();
        createRouteChatRpc();

        TagInfo tagInfo;
        while (true) 
        {
            // Block waiting to read the next event from the completion queue. The
            // event is uniquely identified by its tag, which in this case is the
            // memory address of a CallData instance.
            // The return value of Next should always be checked. This return value
            // tells us whether there is any kind of event or cq_ is shutting down.
            GPR_ASSERT(mCQ->Next((void**)&tagInfo.tagProcessor, &tagInfo.ok)); //GRPC_TODO - Handle returned value
            
            gIncomingTagsMutex.lock();
            gIncomingTags.push_back(tagInfo);
            gIncomingTagsMutex.unlock();

        }
    }

    std::unique_ptr<grpc::ServerCompletionQueue> mCQ;
    routeguide::RouteGuide::AsyncService mRouteGuideService;
    std::vector<routeguide::Feature> mFeatureList;
    std::unique_ptr<grpc::Server> mServer;
};



static void processRpcs()
{
    // Implement a busy-wait loop. Not the most efficient thing in the world but but would do for this example
    while (true)
    {
        gIncomingTagsMutex.lock();
        TagList tags = std::move(gIncomingTags);
        gIncomingTagsMutex.unlock();

        while (!tags.empty())
        {
            TagInfo tagInfo = tags.front();
            tags.pop_front();
            (*(tagInfo.tagProcessor))(tagInfo.ok);

            randomSleepThisThread(); //Simulate processing Time
        };
        randomSleepThisThread(); //yield cpu
    }
}

int main(int argc, char** argv) {
  // Expect only arg: --db_path=<route_guide_db.json path>
  gDB = routeguide::GetDbFileContent(argc, argv);

  std::thread processorThread(processRpcs);

  gpr_set_log_verbosity(GPR_LOG_SEVERITY_DEBUG);

  ServerImpl server;
  gServerImpl = &server;
  server.Run();

  return 0;
}