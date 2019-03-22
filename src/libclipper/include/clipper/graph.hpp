#ifndef CLIPPER_LIB_GRAPH_HPP
#define CLIPPER_LIB_GRAPH_HPP

#include<iostream> 
#include <list> 
#include <stack>
#include <string> 
#include <vector>


#include "constants.hpp"

namespace clipper{


class Vertex
{
    public:
        int vertex_id; //vertex id start from 0
        std::string model_name;

        std::string host_ip;
        int model_port;
        int proxy_port;

        Vertex (std::string modelName): model_name(modelName), host_ip(LOCAL_HOST_IP), model_port(DEFAULT_PORT_NUM), proxy_port(DEFAULT_PORT_NUM){};
        Vertex (std::string modelName, int vertexId):model_name(modelName), vertex_id(vertexId){};
        void setVertexId(int vid);
    
};

// Data structure to store graph edges
struct Edge {
	Vertex src, dst;
};

// Class to represent a graph object
class Graph
{
public:	
	// construct a vector of vectors to represent an adjacency list
	vector<vector<int>> adjList;

    int vertexNodeNumber;

    vector<Vertex> vertexList;

	// Graph Constructor
	Graph(vector<Edge> const &edges, int N);

    void addEdge(Edge edge);

    void addVertex(Vertex v);

    bool isVertexExist(Vertex v);

    static std::string serializeGraphToString(Graph graph);

    static Graph deserializeStringToGraph(std::string);

    static std::string serializeGraphToProtobuf(Graph graph);

};

} // end namespace clipper 


#endif