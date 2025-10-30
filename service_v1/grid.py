# energy grid starter code
from time import sleep
import threading

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
        self.running = False

    # power generation cycle
    def power_generator(self):
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
    def draw_power(self, service_name: str, amount: int):
        with self.lock:
            
            # enough power available from this cycle's production
            if amount <= self.cycle_power:
                self.cycle_power -= amount
                print(f'[{service_name.upper()}] Drew {amount} power from current cycle. Remaining in cycle: {self.cycle_power} power.')
                return True
            
            # not enough power from this cycle's production, use some battery power
            elif amount <= self.cycle_power + self.battery_power:
                self.battery_power -= amount - self.cycle_power
                self.cycle_power = 0
                print(f'[{service_name.upper()}] Falling back to battery power. Current cycle exhausted. Remaining in battery: {self.battery_power} power.')
                return True
            
            # not enough power available from this cycle or from battery
            else:
                print(f'[{service_name.upper()}] Failed to draw power, insufficient power available.')
                return False

    # start energy grid
    def start_grid(self):
        self.running = True
        self.service = threading.Thread(target=self.power_generator, daemon=True)
        self.service.start()

