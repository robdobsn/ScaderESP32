import requests
import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

class HttpStats:
    def __init__(self):
        self.success = 0
        self.test_count = 0
        self.timings = []

httpStats = HttpStats()

def parse_args():
    parser = argparse.ArgumentParser(
        description="Test HTTP server handles"
    )
    # Add ip address and port
    parser.add_argument('url', help="URL to test")
    return parser.parse_args()

def main():
    args = parse_args()
    print(args)

    # Check how many http handles can be opened before failure
    # generate a bar chart of the results
    stats = {}
    for i in range(100):
        try:
            print(f"Opening handle {i}")
            # Split url into host/port and path
            # url = urllib.parse.urlparse(f"http://{args.url}")
            # print(f"{i}: {url.netloc} {url.path}")
            # conn = http.client.HTTPConnection(f"{url.netloc}")
            # conn.request("GET", f"{url.path}")
            # response = conn.getresponse()
            # print(f"{i}: Response status: {response.status}")
            # if response.status != 200:
            #     break
            resp = requests.get(f"http://{args.url}")
            httpStats.test_count += 1
            if resp.status_code == 200:
                httpStats.success += 1
            httpStats.timings.append(resp.elapsed.total_seconds())
            # print(f"{i}: Response status: {resp.status_code} elapsed: {resp.elapsed}")

        except Exception as e:
            print(f"Failed to open handle {i}: {e}")
            break

    print(f"Success: {httpStats.success}/{httpStats.test_count} ({httpStats.success/httpStats.test_count*100:.2f}%)")
    # df = pd.DataFrame({"timings":httpStats.timings})
    df = pd.DataFrame(httpStats.timings)
    plt.hist(df.values, edgecolor="k")
    plt.show()

if __name__ == "__main__":
    main()
