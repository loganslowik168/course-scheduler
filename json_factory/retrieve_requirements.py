#!/usr/bin/env python3
import sys
import requests

def fetch_website(url):
    try:
        # We add a User-Agent header so the request looks like it's coming from a browser
        headers = {
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
        }
        
        # Send the GET request
        response = requests.get(url, headers=headers, timeout=10)
        
        # Raise an exception for 4xx or 5xx status codes
        response.raise_for_status()
        
        # Print the text content of the page
        print(response.text)
        
    except requests.exceptions.HTTPError as http_err:
        print(f"HTTP error occurred: {http_err}")
    except requests.exceptions.ConnectionError:
        print("Error: Could not connect to the server. Check the URL or your internet.")
    except requests.exceptions.Timeout:
        print("Error: The request timed out.")
    except requests.exceptions.RequestException as err:
        print(f"An error occurred: {err}")

if __name__ == "__main__":
    # Ensure a URL was provided as a runtime argument
    if len(sys.argv) != 2:
        print("Usage: python web_fetch.py <URL>")
        sys.exit(1)
    
    target_url = sys.argv[1]
    
    # Simple check to ensure the URL starts with http
    if not target_url.startswith(("http://", "https://")):
        target_url = "https://" + target_url
        
    fetch_website(target_url)