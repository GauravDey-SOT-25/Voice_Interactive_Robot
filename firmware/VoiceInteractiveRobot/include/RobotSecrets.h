#pragma once

#if __has_include("Secrets.h")
#include "Secrets.h"
#else
#error "Copy include/Secrets.example.h to include/Secrets.h and configure Wi-Fi, HTTPS endpoint, token, and root CA."
#endif
