const https = require('https');

exports.handler = function(event, context) {
 var longitude = event.longitude;
 var latitude  = event.latitude;
 var date      = event.date;
 var past      = event.past;
 var interval  = event.interval;
 var clientid  = event.clientid;

// var restPath = '/weather/V1/place?appid=' + process.env.YOLP_APPID
 var restPath = '/weather/V1/place?appid=' + clientid
  + '&coordinates=' + longitude + ',' + latitude + '&output=json'
  + (date     && date.length     ? '&date='     + date : '')
  + (past     && past.length     ? '&past='     + past : '')
  + (interval && interval.length ? '&interval=' + interval : '');

 var options = {
  protocol: 'https:',
  host: 'map.yahooapis.jp',
  path: restPath,
  method: 'GET',
 };

 const req = https.request(options, (res) => {
  var body = '';
  res.setEncoding('utf8');
  res.on('data', (chunk) => { body += chunk; });
  res.on('end', () => {
   const json = JSON.parse(body);
   const weatherData = json.Feature[0].Property.WeatherList.Weather.map((element) => {
	return (element.Type == 'observation' ? 'o' : (element.Type == 'forecast' ? 'f' : 'x')) + element.Rainfall;
   });
/*   const weatherData = json.Feature[0].Property.WeatherList.Weather.map(weather => weather.Date + '|' + weather.Rainfall + '|'
    + (weather.Type == 'observation' ? 'o' : (weather.Type == 'forecast' ? 'f' : weather.Type)));*/
/*   var weatherData = '';
   json.Feature[0].Property.WeatherList.Weather.forEach((weather) => {
	weatherData += weather.Date + '|' + weather.Rainfall + '|' + weather.Type;
   });*/
//   const weatherJson = JSON.stringify(weatherData);
   context.succeed(weatherData);
  });
 });

 req.on('error', (e) => {
  console.error(`problem with request: ${e.message}`);
 });

 req.end();
};

/* api gateway's mapping template : path = /{longitude}/{latitude}, method = GET, Content-Type = application/json
{
 "longitude": "$input.params('longitude')",
 "latitude":  "$input.params('latitude')",
 "past":      "$input.params('past')",
 "interval":  "$input.params('interval')",
 "clientid":  "$input.params('id')"
}*/

/*exports.handler = async (event) => {
    // TODO implement
    const response = {
        statusCode: 200,
        body: JSON.stringify('Hello from Lambda!'),
    };
    return response;
};*/
