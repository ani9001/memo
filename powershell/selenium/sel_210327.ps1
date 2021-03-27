$hubHost = "127.0.0.1"
$queryString = "HelloWorld"

$webDriverDllPath = ".\WebDriver.dll"

$opts = $env:BAT_ARGS -split '(".*?")|\s+' -ne ""
foreach ($opt in $opts) {
 if ($opt.IndexOf(":") -ge 0) { $optpair = $opt -split ":", 2 } else { $optpair = $opt -split "=", 2 }
 if ($optpair.Length -eq 2) {
  switch ($optpair[0]) {
   "/hub"	{ $hubHost		= $optpair[1] }
   "/query"	{ $queryString	= $optpair[1] }
  } } }

Add-Type -Path $webDriverDllPath
if ($hubHost.IndexOf(":") -lt 0) { $hubHost += ":4444" }
$seleniumGridHub = New-Object System.Uri("http://" + $hubHost + "/wd/hub")

$capabilities = New-Object OpenQA.Selenium.Remote.DesiredCapabilities
$capabilities.SetCapability("browserName", "firefox")
$ffDriver = New-Object OpenQA.Selenium.Remote.RemoteWebDriver($seleniumGridHub, $capabilities)
$ffDriver.Manage().Window.Maximize()

$ffDriver.Url = "http://www.google.co.jp/"

# findElementByName("q"), findElementById("lst-ib")
$ffDriver.findElementByName("q").SendKeys($queryString)
$ffDriver.findElementByName("q").submit()

do { Start-Sleep -s 1 } until ($ffDriver.Title.Contains($queryString))
Write-Host $ffDriver.findElementByClassName("LC20lb").Text

Start-Sleep -s 10
$ffDriver.quit()
