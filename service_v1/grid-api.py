# energy-grid service

from flask import Flask, request, jsonify
import json
import threading
import time

import grid
app = Flask(__name__)

# index route
@app.route('/', methods=['GET'])
def index():
    return "<h1>Welcome to Grid Basic!</h1>"

# handles requests for drawing power from the grid
@app.route('/use', methods=['GET'])
def use():
    try:
        # requests should be sent in format {"service": "<service_name>", "amount": <service_amount>}
        data = request.get_json()
        service_name = data.get('service')
        service_amount = int(data.get('amount'))

        # required request parameter missing
        if not service_name or not service_amount:
            return jsonify({"result": False, "message": "Failed to specify either service name or requested power amount"}), 400
        
        # call grid power drawing function, get result status back and send response in form {"result": <true/false>, "message": <message>}
        result = the_grid.draw_power(service_name, service_amount)
        if result:
            response = {"result": True, "message": f"Successfully drew power for {service_name}"}
            return jsonify(response), 200
        else:
            response = {"result": False, "message": f"Failed to draw power for {service_name}"}
            return jsonify(response), 409
    
    # wrong type sent for service_amount
    except ValueError as e:
        print(f'Debug: API user error {e}')
        return jsonify({"result": False, "message": "Failed to draw power; error in amount request field"}), 400
    
    # handles other bad request errors, this should be updated to be more specific
    except Exception as e:
        print(f'Debug: Unexpected Error {e}')
        return jsonify({"result": False, "message": "Failed to draw power; bad request"}), 400

if __name__ == '__main__':
    the_grid = grid.energy_grid()
    the_grid.start_grid()
    app.run(host='127.0.0.1', port=8080, debug=False, threaded=True, use_reloader=False)
