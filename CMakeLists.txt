#  Copyright (c) Microsoft Corporation. All rights reserved.
#  Licensed under the MIT License.

cmake_minimum_required (VERSION 3.8)
project (azure-sphere-libcurl-awsiotcore C)

# Create executable
add_executable (${PROJECT_NAME} main.c delay.c)
target_link_libraries (${PROJECT_NAME} applibs pthread gcc_s c curl tlsutils)

# macro
add_compile_definitions(AzureSphere_CA7)

# Add MakeImage post-build command
SET(ADDITIONAL_APPROOT_INCLUDES "certs/AmazonRootCA1.pem" "certs/f1314901d2-certificate.pem" "certs/f1314901d2-private.pem")
include (${AZURE_SPHERE_MAKE_IMAGE_FILE})
