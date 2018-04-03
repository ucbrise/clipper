#!/usr/bin/env RScript

print("Installing R container test dependencies...")

install.packages('versions', repos='http://cran.us.r-project.org')
versions::install.versions('jsonlite', version='1.5')
versions::install.versions('Rcpp', version='0.12.11')
versions::install.versions('optparse', version='1.4.4')
versions::install.versions('stringr', version='1.2.0')
versions::install.versions('CodeDepends', version='0.5-3')

