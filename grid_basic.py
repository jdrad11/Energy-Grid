# energy grid starter code
from time import sleep
import threading
import sys

# constant variables
production_rate = 100
cycle_duration = 5
battery_max = 500

# energy grid object
class energy_grid:
    def __init__(self):
        # power produced during a cycle
        self.cycle_power = 0
        # extra power stored after a cycle
        self.battery_power = 0
        # locking mechanism to allow threads to run concurrently
        self.lock = threading.Lock()
        # used to stop energy grid service
        self.running = True

    # power generation cycle
    def generate_power_cycle(self):
        print(f'[GENERATOR] Energy grid generation service started. Producing power every {cycle_duration} seconds.')
        
        # this cycle will run until the power grid is shut down
        while self.running:
            with self.lock:
                self.cycle_power += production_rate
            print(f'[GENERATOR] Generated {production_rate} power.')
            
            # services will request power during this period
            sleep(cycle_duration)
            
            # enforce the maximum battery capacity
            self.battery_power = min(self.battery_power + self.cycle_power, battery_max)
            print(f'[GENERATOR] Used {production_rate-self.cycle_power} power this cycle. Backup battery now storing {self.battery_power} power.\n')
            self.cycle_power = 0
        
        print('[GENERATOR] Energy Grid shutting down now...')

    # draws power from the grid
    def use_power_service(self, service_name: str, amount: int):
        with self.lock:
            
            # enough power available from this cycle's production
            if amount <= self.cycle_power:
                self.cycle_power -= amount
                print(f'[{service_name.upper()}] Drew {amount} power from current cycle. Remaining in cycle: {self.cycle_power} power.')
            
            # not enough power from this cycle's production, use some battery power
            elif amount <= self.cycle_power + self.battery_power:
                self.battery_power -= amount - self.cycle_power
                self.cycle_power = 0
                print(f'[{service_name.upper()}] Falling back to battery power. Current cycle exhausted. Remaining in battery: {self.battery_power} power.')
            
            # not enough power available from this cycle or from battery
            else:
                print(f'[{service_name.upper()}] Failed to draw power, insufficient power available.')

    # start energy grid
    def start_grid(self):
        self.service = threading.Thread(target=self.generate_power_cycle, daemon=True)
        self.service.start()

    # stop energy grid
    def stop_grid(self):
        self.running = False

# simulate running a service that reaches out and draws power
def run_power_drawing_service(grid: energy_grid, service_name: str, amount: int):
    print(f'[{service_name.upper()}] Service simulator started.')
    
    # this cycle will run until the service is shut down
    while grid.running:
        grid.use_power_service(service_name, amount)
        sleep(cycle_duration)
    
    print(f'[{service_name.upper()}] Service simulator shutting down now...')

# run the energy grid service
def run_energy_grid():
    # spawn the energy grid object
    grid = energy_grid()
    grid.start_grid()
    
    # spawn the services that will reach out to the energy grid for power
    services = []
    service_a = threading.Thread(target=run_power_drawing_service, args=(grid, 'Data Center', 25), daemon=True)
    service_b = threading.Thread(target=run_power_drawing_service, args=(grid, 'Drawbridge', 25), daemon=True)
    service_c = threading.Thread(target=run_power_drawing_service, args=(grid, 'Radios', 25), daemon=True)
    services.append(service_a)
    services.append(service_b)
    services.append(service_c)
    for s in services:
        s.start()
    
    # keep the main thread running until the energy grid thread is shut down
    # for this simulation, use Ctrl+C to shut down the energy grid - lazy :( so fix this later
    try:
        while True:
            sleep(cycle_duration)
    except KeyboardInterrupt:
        grid.stop_grid()
        # sleep a final cycle duration to let everything shut down
        sleep(cycle_duration)
        # ensure service simulation threads are closed
        for s in services:
            if s.is_alive():
                s.join(timeout=1)
        print('\nEnergy Grid killed by Ctrl+C')

# to help make this portable into a service later
if __name__ == '__main__':
    run_energy_grid()
