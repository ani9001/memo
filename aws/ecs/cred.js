// common
AWS.config.update({
 accessKeyId:    'xxxxxxxxxxx',
 secretAccessKey:'xxxxxxxxxxxxxxxxxxxxxxxxx',
 region:         'ap-northeast-1'
});

sgID   = "sg-xxxxxxxxxxxxxxxx";
getgIP = "https://xxxxxxxx.execute-api.ap-northeast-1.amazonaws.com/prod";

// ecs
serviceName = "xxxx-selenium"
clusterName = "xxxx-selenium"
