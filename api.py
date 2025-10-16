from flask import Flask
import threading
import time

import grid_basic
app = Flask(__name__)

@app.route('/')
def index():
    return "<h1>Welcome to Grid Basic!</h1>"

@app.route('/use')
def use():
    worker = threading.Thread(target=the_grid.use_power_service("Test", 15))
    worker.start()
    return "<h1>Consuming power</h1>"

if __name__ == '__main__':
    the_grid = grid_basic.energy_grid()
    the_grid.start_grid()
    app.run(host='127.0.0.1', port=8080, debug=False)