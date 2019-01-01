# Description of CI process

The wonderful AMPlab Jenkins and Travis CI is responsible for running our integration test.

## How does the CI process work (version 2.0)
- There are three components of Clipper CI:
    1. Shipyard: generate Makefiles for both building and testing.
    2. Jenkins: build, push, and test containers. It will run all tests except kubernetes test. 
    3. Travis: run kubernetes test. Travis will wait for Jenkins finish building it's required containers, using `wait_kubernetes_containers` Makefile target. 
- Rationale:
    1. We want to build the images in parallel as much as we can, however, our images have complex dependencies. Makefile is our best bet. However, one does not simply write complex Makefile. We built our in-house, light-weight makefile generator _shipyard_ to provide a declarative and pythonic way to generate the file. 
    2. Jenkins is our main CI worker. It simply set up the environment and run whatever makefile provides. 
    3. One downside of Jenkins is that it only runs docker, not VM. Minikube is a scarce resource. For this reason, Travis is used because it provides a VM. 
- Other tooling:
    1. `colorize_output.py` script in the `bin/` directory is used to tag each line of the log output. 
    1. [A webapp](http://clipper-jenkins-viewer.herokuapp.com/) deployed at heroku can be used to disaggregate the logs and visualize Makefile dag. 


## How does the CI process work (version 1.0)

0. Jenkins pull the PR.
1. Jenkins inject environment variables configured in admin page. Currently, we set the following environment variables 
in Pull Request Builder:
    - AWS_ACCESS_KEY_ID
    - AWS_SECRET_ACCESS_KEY
    - CLIPPER_K8S_CERT_AUTH
    - CLIPPER_K8S_CLIENT_CERT
    - CLIPPER_K8S_CLIENT_KEY
    - CLIPPER_K8S_PASSWORD
    - CLIPPER_TESTING_DOCKERHUB_PASSWORD
2. Jenkins will call `run_ci.sh`. It does three things:
    - It calls `build_docker_images.sh` to build all the docker images
    - Then it runs unittests docker container for python2 and python3
    - Each unittest cotnainer will run `ci_checks.sh`
3. `ci_checks.sh` will run two things:
    - (Only in Python2) It runs `check_foramt.sh` to run the linter for C++ and Python
    - It runs `run_unnitests.sh` to run all tests. 
        - (Only in Python3) It will only run the integration test part, which contains all the python tests and R tests
        
## Note on Minikube
- We are under the process of moving away from AWS EKS to Minikube in our CI process. Once the PR is in, there will be 
more detail here. 
