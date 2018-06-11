# Description of CI process

The wonderful AMPlab Jenkins is responsible for running our integration test.

## How does the CI process work


0. Jenkins pull the PR and sandbox it.
1. Jenkins inject environment variables configured in admin page
2. Jenkins will call `run_ci.sh`. It does three things:
    - It calls `build_docker_images.sh` to build all the docker images
    - Then it runs unittests docker container for python2 and python3
    - Each unittest cotnainer will run `ci_checks.sh`
3. `ci_checks.sh` will run two things:
    - (Only in Python2) It runs `check_foramt.sh` to run the linter for C++ and Python
    - It runs `run_unnitests.sh` to run all tests. 
        - (Only in Python3) It will only run the integration test part, which contains all the python tests and R tests
        
## Note on Minikube (WIP)
- We are under the process of moving away from AWS EKS to Minikube in our CI process. Once the PR is in, there will be 
more detail here. 