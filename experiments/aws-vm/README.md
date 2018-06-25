## Introduction

This vagrant file contains instructions to spawn instances on AWS of various distros. When possible ami-id are grabbed from aws to be sure they are the latest versions available. 

## Usage

To run this script you need to install vagrant first of all, then vagrant-aws plugin and setup your AWS credentials:

```
$ export AWS_ID="<yourid>"
$ export AWS_SECRET="<secret>"
$ export AWS_SGS="<security_group_id>"[,"<security_group_id>"]*
$ export AWS_SUBNET="<subnet_id>"
$ export PEM_PATH="path_to_testinfrastructure.pem"

Optional:
$ export AWS_PREFIX="<your initials>"
```

Then, add the aws plugin:
```
$ vagrant plugin install vagrant-aws
...
```
and the dummy box, using any name you want:
```
$ vagrant box add dummy https://github.com/mitchellh/vagrant-aws/raw/master/dummy.box
...
```

Then you can list available distro with:

```
$ vagrant status
squeeze64                 not created (aws)
wheezy64                  not created (aws)
jessie64                  not created (aws)
fedora21_64               not created (aws)
fedora22_64               not created (aws)
centos5_64                not created (aws)
centos6_64                not created (aws)
centos6_32                not created (aws)
centos7_64                not created (aws)
amazon64                  not created (aws)
rhel5                     not created (aws)
rhel7                     not created (aws)
rhel7atomic               not created (aws)
sles12                    not created (aws)
coreos-stable             not created (aws)
coreos-beta               not created (aws)
coreos-alpha              running (aws)
zesty64                   not created (aws)
xenial64                  not created (aws)
precise64                 not created (aws)
yakkety64                 not created (aws)
trusty64                  not created (aws)
```

and launch one of them with:

```
$ vagrant up coreos-alpha
$ vagrant ssh coreos-alpha
```

To Undeploy/Terminate the instance you are working on:

```
$ vagrant destroy -f *INSTANCE_NAME*