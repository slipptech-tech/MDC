curl --location --fail --show-error --silent \
  --request GET \
  "https://github.com/slipptech-tech/MDC/archive/refs/heads/main.zip" \
  --header "User-Agent: MDC-Downloader/1.0" \
  --header "Accept: application/octet-stream" \
  --header "Accept-Encoding: gzip, deflate, br" \
  --connect-timeout 20 \
  --max-time 180 \
  --retry 5 \
  --retry-all-errors \
  --retry-delay 3 \
  --output "MDC-main.zip"