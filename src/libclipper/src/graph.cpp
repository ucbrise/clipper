#include <vector>


#include <clipper/graph.hpp>


namespace clipper{

void Vertex::setVertexId(int vid){
    vertex_id=vid;
};

Graph::Graph(vector<Edge> const &edges, int N)
{
		// resize the vector to N elements of type vector<int>
	adjList.resize(N);

		// add edges to the Directed graph
		for (auto &edge: edges){
            if(!isVertexExist(edge.src)){
                addVertex(edge.src);
            }

            if(!isVertexExist(edge.dst)){
                addVertex(edge.dst);
            }
			adjList[edge.src.vertex_id].push_back(edge.dest.vertex_id);
        }
};

void Graph::addEdge(Edge edge){

        adjList.resize(adjList.size+1);

        if(!isVertexExist(edge.src)){
            addVertex(edge.src);
        }
        if(!isVertexExist(edge.dst)){
            addVertex(edge.dst);
        }
		adjList[edge.src.vertex_id].push_back(edge.dest.vertex_id);

    };

void Graph::addVertex(Vertex v){
        vertexList.resize(vertexList.size+1);
        vertexList.push_back(v);
};

bool Graph::isVertexExist(Vertex v){
        for (auto &w: vertexList){
            if (v.model_name == w.model_name)
                return true;
        }
        return false;
};


std::string Graph::serializeGraphToString(Graph graph){

        return "";

};

Graph Graph::deserializeStringToGraph(std::string){

        

};

std::string Graph::serializeGraphToProtobuf(Graph graph){
        return "";
};

} // namespace clipper