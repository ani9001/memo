$hubHost = "127.0.0.1"
$queryString = "HelloWorld"
$ssFilePath = ""

$webDriverDllPath        = ".\WebDriver.dll"
$webDriverSupportDllPath = ".\WebDriver.Support.dll"

$opts = $env:BAT_ARGS -split '(".*?")|\s+' -ne ""
foreach ($opt in $opts) {
 if ($opt.IndexOf(":") -ge 0) { $optpair = $opt -split ":", 2 } else { $optpair = $opt -split "=", 2 }
 if ($optpair.Length -eq 2) {
  switch ($optpair[0]) {
   "/hub"	{ $hubHost		= $optpair[1] }
   "/query"	{ $queryString	= $optpair[1] }
   "/ss"	{ $ssFilePath	= $optpair[1] }
  } } }

Add-Type -Path $webDriverDllPath
Add-Type -Path $webDriverSupportDllPath
if ($hubHost.IndexOf(":") -lt 0) { $hubHost += ":4444" }
$seleniumGridHub = New-Object System.Uri("http://" + $hubHost + "/wd/hub")

$capabilities = New-Object OpenQA.Selenium.Remote.DesiredCapabilities
$capabilities.SetCapability("browserName", "firefox")
$ffDriver = New-Object OpenQA.Selenium.Remote.RemoteWebDriver($seleniumGridHub, $capabilities)
$ffDriver.Manage().Window.Maximize()

$ffDriver.Navigate().GoToUrl("https://www.google.com/")
$ffDriver.findElementByName("q").SendKeys($queryString + [OpenQA.Selenium.Keys]::Enter)

$webDriverWait = New-Object OpenQA.Selenium.Support.UI.WebDriverWait($ffDriver, (New-TimeSpan -Seconds 10))
$null = $webDriverWait.until([OpenQA.Selenium.Support.UI.ExpectedConditions]::TitleContains($queryString))

foreach ($element in $ffDriver.findElementsByClassName("LC20lb")) {
 Write-Host $element.Text
}

if ($ssFilePath.Length -ge 1) {
 $screenShot = [OpenQA.Selenium.Support.Extensions.WebDriverExtensions]::TakeScreenshot($ffDriver)
 $imageFormat = [OpenQA.Selenium.ScreenshotImageFormat]::Png
 $screenShot.SaveAsFile($ssFilePath, $imageFormat)
}

Start-Sleep -s 1
$ffDriver.quit()
