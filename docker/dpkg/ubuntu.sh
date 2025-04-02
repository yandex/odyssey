#!/usr/bin/env bash

set -ux

codename="bionic"
ldap_version=""
output_folder="packages"
while getopts ":hc:l:o:" option; do
    case $option in
        c)
            codename=$OPTARG;;
        l)
            ldap_version=$OPTARG;;
        o)
            output_folder=$OPTARG;;
        h)
            cat << EOM
$(basename "$0") [-h] [-c codename] [-l ldap-version] [-o output folder] -- build odyssey deb in docker and save packages on host
where:
    -h  show this help text
    -c  codename of ubuntu distro (bionic, jammy, etc)
    -l  libldap-dev package verison
    -o  output folder
EOM
            exit 1;;
        \?)
            echo "Error: Invalid option"
            exit 1;;
   esac
done

docker build . --tag odyssey_dpkg -f ./docker/dpkg/Dockerfile --build-arg codename="$codename" --build-arg libldap_version="$ldap_version"
cid=`docker create odyssey_dpkg:latest`
docker cp ${cid}:/odyssey/packages "$output_folder"
docker rm ${cid}
