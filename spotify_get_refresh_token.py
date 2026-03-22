"""
Spotify Refresh Token Helper for Chuha Pie 1.0
================================================
Run this script ONCE on your computer to get a refresh token.
Then paste the refresh token into Main/secrets.h

Steps:
1. Go to https://developer.spotify.com/dashboard
2. Create an app, set redirect URI to http://127.0.0.1:8888/callback
3. Copy Client ID and Client Secret
4. Run this script: python spotify_get_refresh_token.py
5. Paste the refresh token into the .ino file
"""

import http.server
import urllib.parse
import webbrowser
import requests
import json
import sys

CLIENT_ID = ""
CLIENT_SECRET = ""

REDIRECT_URI = "http://127.0.0.1:8888/callback"
SCOPES = "user-read-playback-state user-modify-playback-state user-read-currently-playing"
AUTH_URL = "https://accounts.spotify.com/authorize"
TOKEN_URL = "https://accounts.spotify.com/api/token"

auth_code = None


class CallbackHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        global auth_code
        query = urllib.parse.urlparse(self.path).query
        params = urllib.parse.parse_qs(query)

        if "code" in params:
            auth_code = params["code"][0]
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(
                b"<html><body><h1>Success! You can close this tab.</h1>"
                b"<p>Go back to the terminal to copy your refresh token.</p></body></html>"
            )
        elif "error" in params:
            self.send_response(400)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(b"<html><body><h1>Authorization failed.</h1></body></html>")
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        pass  # Suppress default logging


def main():
    global CLIENT_ID, CLIENT_SECRET

    print("=" * 50)
    print("Spotify Refresh Token Helper — Chuha Pie")
    print("=" * 50)
    print("Get your Client ID and Secret from:")
    print("https://developer.spotify.com/dashboard")
    print()
    CLIENT_ID = input("Enter your Client ID: ").strip()
    CLIENT_SECRET = input("Enter your Client Secret: ").strip()
    print()

    # Build authorization URL
    auth_params = {
        "client_id": CLIENT_ID,
        "response_type": "code",
        "redirect_uri": REDIRECT_URI,
        "scope": SCOPES,
    }
    url = AUTH_URL + "?" + urllib.parse.urlencode(auth_params)

    print("Opening browser for Spotify login...")
    print(f"If it doesn't open, go to:\n{url}\n")
    webbrowser.open(url)

    # Start local server to catch the callback
    server = http.server.HTTPServer(("localhost", 8888), CallbackHandler)
    print("Waiting for authorization...")
    server.handle_request()

    if not auth_code:
        print("No authorization code received. Exiting.")
        sys.exit(1)

    # Exchange auth code for tokens
    print("Exchanging code for tokens...")
    response = requests.post(
        TOKEN_URL,
        data={
            "grant_type": "authorization_code",
            "code": auth_code,
            "redirect_uri": REDIRECT_URI,
            "client_id": CLIENT_ID,
            "client_secret": CLIENT_SECRET,
        },
    )

    if response.status_code != 200:
        print(f"Token exchange failed: {response.status_code}")
        print(response.text)
        sys.exit(1)

    tokens = response.json()

    print("\n" + "=" * 50)
    print("SUCCESS! Copy this refresh token into your .ino file:")
    print("=" * 50)
    print(f"\n{tokens['refresh_token']}\n")
    print("=" * 50)
    print(f"Access token (expires in {tokens.get('expires_in', '?')}s):")
    print(tokens["access_token"][:40] + "...")
    print("=" * 50)
    print("\nPaste the refresh token as SPOTIFY_REFRESH_TOKEN in")
    print("Main/secrets.h and you're good to go!")


if __name__ == "__main__":
    main()
