@echo off
cd /d "%~dp0"

(echo version: '3'
 echo services:
 echo  web:
 echo   image: httpd:alpine
 echo   ports:
 echo    - "8080:80"
 echo   volumes:
 echo    - C:\Users\xxx\Downloads:/usr/local/apache2/htdocs/
)>docker-compose.yml

docker-compose up -d

echo 起動しました. 終了するにはなにかキーを押してください.
pause >nul

docker-compose stop
docker-compose down --rmi all
del /q docker-compose.yml >nul
