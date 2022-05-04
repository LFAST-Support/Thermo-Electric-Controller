REM using wsl2 to host brokers and test client due to unresolved issues with Rory's Windows environment
@REM SET LINUX_IP=%1

@REM if %~1=="" goto :end
@REM netsh interface portproxy add v4tov4 listenaddress=192.168.1.9 listenport=1884 connectaddress=127.0.0.1 connectport=1884
@REM netsh interface portproxy add v4tov4 listenaddress=192.168.1.9 listenport=1883 connectaddress=127.0.0.1 connectport=1883
@REM netsh interface portproxy add v4tov4 listenaddress=0.0.0.0 listenport=1883 connectaddress=%LINUX_IP% connectport=1883
@REM netsh interface portproxy add v4tov4 listenaddress=0.0.0.0 listenport=1884 connectaddress=%LINUX_IP% connectport=1884
@REM :end

@REM netsh interface portproxy delete v4tov4 listenaddress=192.168.1.9 listenport=1884
@REM netsh interface portproxy delete v4tov4 listenaddress=192.168.1.9 listenport=1883

@REM taskkill /F /FI "SERVICES eq iphlpsvc"
@REM need to kill iphlpsvc (aka IP Helper) from the services console if it's interfering
start powershell {mosquitto -c broker1.config; Read-Host}
start powershell {mosquitto -c broker2.config; Read-Host}

@REM Example command to start the Test Client:
@REM py test_client.py

@REM Use Windows Subsystem for Linux (WSL) to start up using Linux commands natively on Windows
@REM wsl.exe -d Ubuntu-20.04 cd adc_test_env;./start_test_env.sh
