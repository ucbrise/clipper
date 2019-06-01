import argparse

import boat_cluster

if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument('-n', '--num_nodes', type=int, default=3)
    args = parser.parse_args()

    boat_cluster = boat_cluster.BoatCluster(args.num_nodes)
    boat_cluster.run()

    exit(0)