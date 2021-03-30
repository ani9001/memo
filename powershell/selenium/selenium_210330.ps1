$hubHost = "127.0.0.1"
$asin = "B00O2TXF8O"
$ssFilePath = ""

$webDriverDllPath        = ".\WebDriver.dll"
$webDriverSupportDllPath = ".\WebDriver.Support.dll"

$opts = $env:BAT_ARGS -split '(".*?")|\s+' -ne ""
foreach ($opt in $opts) {
 if ($opt.IndexOf(":") -ge 0) { $optpair = $opt -split ":", 2 } else { $optpair = $opt -split "=", 2 }
 if ($optpair.Length -eq 2) {
  switch ($optpair[0]) {
   "/hub"	{ $hubHost		= $optpair[1] }
   "/asin"	{ $asin			= $optpair[1] }
   "/ss"	{ $ssFilePath	= $optpair[1] }
  } } }

Add-Type -Path $webDriverDllPath
Add-Type -Path $webDriverSupportDllPath
if ($hubHost.IndexOf(":") -lt 0) { $hubHost += ":4444" }
$seleniumGridHub = New-Object System.Uri("http://" + $hubHost + "/wd/hub")

$capabilities = New-Object OpenQA.Selenium.Remote.DesiredCapabilities
$capabilities.SetCapability("browserName", "firefox")

do {
 $ffDriver = New-Object OpenQA.Selenium.Remote.RemoteWebDriver($seleniumGridHub, $capabilities)
 $ffDriver.Manage().Window.Maximize()

 $ffDriver.Navigate().GoToUrl("https://www.amazon.co.jp/dp/" + $asin + "/")

 $webDriverWait = New-Object OpenQA.Selenium.Support.UI.WebDriverWait($ffDriver, (New-TimeSpan -Seconds 10))

 $SavedErrorActionPreference = $ErrorActionPreference
 $ErrorActionPreference = "silentlyContinue"
 if($webDriverWait.Until([OpenQA.Selenium.Support.UI.ExpectedConditions]::ElementExists([OpenQA.Selenium.By]::Id("priceblock_ourprice")))) {
  Write-Host ((Get-Date).ToString() + " Price: " + $ffDriver.FindElementsById("priceblock_ourprice").Text)
 }
 $ErrorActionPreference = $SavedErrorActionPreference

 $webDriverWait.Finalize

 if ($ssFilePath.Length -ge 1) {
  $screenShot = $ffDriver.GetScreenshot()
  $imageFormat = [OpenQA.Selenium.ScreenshotImageFormat]::Png
  $screenShot.SaveAsFile($ssFilePath, $imageFormat)
 }
 $ffDriver.quit()

 Start-Sleep -s 60
} while ($true)
