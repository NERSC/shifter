#!/usr/bin/env python
from shifter_imagegw import api

LISTEN_PORT=5000

if __name__ == "__main__":
    api.app.run(debug=api.DEBUG_FLAG, host='0.0.0.0', port=LISTEN_PORT, threaded=True)
