// ATTENTION
/*
    Steps to be followed:
    1. After cloning the Repo - Rename this file as secrets.h
    2. Update the WIFI (2.4GHz band) credentials
    3. Further, follow the steps written below
*/

#pragma once


// ============ WIFI CREDENTIALS ============
// Replace with your WiFi network name and password (must be 2.4GHz)
#define WIFI_SSID_VAL     "Your WiFi Name"
#define WIFI_PASSWORD_VAL "Your WiFi Password"


// ============ SPOTIFY CREDENTIALS ============
// Step 1: Create an app at https://developer.spotify.com/dashboard
//         Set redirect URL to: http://localhost:8888/callback
// Step 2: Copy your Client ID and Client Secret below
// Step 3: Run spotify_get_refresh_token.py from the repo to get your Refresh Token

#define SPOTIFY_CLIENT_ID_VAL     "paste_client_id_here"
#define SPOTIFY_CLIENT_SECRET_VAL "paste_client_secret_here"
#define SPOTIFY_REFRESH_TOKEN_VAL "paste_refresh_token_here"