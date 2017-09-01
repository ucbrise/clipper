Dockerfiles for the Clipper project.

Note that while these Dockerfiles are in the `dockerfiles` subdirectory, they must be built with the root
of the Clipper repo as the build context. For example:

*From within this directory*
```
docker build -t <tag> -f QueryFrontendDockerfile ../
```

*From the Clipper root directory*
```
docker build -t <tag> -f dockerfiles/QueryFrontendDockerfile ./
```
