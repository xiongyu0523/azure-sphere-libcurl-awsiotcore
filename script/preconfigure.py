import os
import boto3
from asn1crypto import pem, x509
from subprocess import run, PIPE

azsphere_program = r'C:\Program Files (x86)\Microsoft Azure Sphere SDK\Tools\azsphere '
ioTPolicyName = 'IoTSimplePolicy'
iotclient = boto3.client('iot')
CA_FILE_NAME = 'ca.cer'
VER_FILE_NAME = 'ver.cer'

def utility_verison():
    result = run(azsphere_program + 'show-version', capture_output=True)
    return result.stdout.decode('utf-8')

def utility_download_ca_certificate(file_name):
    result = run(azsphere_program + 'tenant download-ca-certificate -o ' + file_name, capture_output=True)
    str = result.stdout.decode('utf-8')
    return True if 'Saved the CA certificate' in str else False    

def utility_download_validation_certificate(code, file_name):
    result = run(azsphere_program + 'tenant download-validation-certificate -c ' + code + ' -o ' + file_name, capture_output=True)
    str = result.stdout.decode('utf-8')
    return True if 'Saved the validation certificate' in str else False    

def ca_certificate_register():

    if utility_verison() < '19.11':
        print('ERROR: update Azure Sphere SDK to the latest version')
        return

    response = iotclient.get_registration_code()
    regCode = response['registrationCode']

    if os.path.exists(CA_FILE_NAME):
        os.remove(CA_FILE_NAME)

    if os.path.exists(VER_FILE_NAME):
        os.remove(VER_FILE_NAME)

    if not utility_download_ca_certificate(CA_FILE_NAME):
        print('ERROR: failed to download ca certificate')
        return

    if not utility_download_validation_certificate(regCode, VER_FILE_NAME):
        print('ERROR: failed to download validation certificate')
        return

    with open(CA_FILE_NAME, 'rb') as f:
        ca_pem = pem.armor('CERTIFICATE', f.read())

    with open(VER_FILE_NAME, 'rb') as f:
        ver_pem = pem.armor('CERTIFICATE', f.read())

    try:
        response = iotclient.register_ca_certificate(caCertificate=ca_pem.decode('utf-8'), verificationCertificate=ver_pem.decode('utf-8'), setAsActive=True, allowAutoRegistration=True)
    except iotclient.exceptions.ResourceAlreadyExistsException:
        print("INFO: Already have this CA registered")
    else:
        print("INFO: A new CA certificate is registered")

def create_iot_policy():
    try:
        response = iotclient.create_policy(
            policyName=ioTPolicyName, 
            policyDocument="{\"Version\":\"2012-10-17\",\"Statement\":[{\"Effect\":\"Allow\",\"Action\":\"iot:*\",\"Resource\":\"*\"}]}"
        )
    except iotclient.exceptions.ResourceAlreadyExistsException:
        print("INFO: Already have this policy")
    else:
        print("INFO: A new policy is created")

if __name__ == "__main__":

    ca_certificate_register()
    create_iot_policy()