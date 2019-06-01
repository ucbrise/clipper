import argparse, multiprocessing, logging, signal, os, datetime, subprocess, time

from boat import Boat

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

class BoatCluster:
    def __init__(self, num_nodes):
        self.num_nodes = num_nodes
        self.start_time = datetime.datetime.now()
        self.processes = list()
    
    # Boat failure recovery
    def sigchld_handler(self, signo, frame):
        # Avoid infinite loop while handling this failure
        signal.signal(signal.SIGCHLD, signal.SIG_DFL)

        all_alive = True
        for i in range(len(self.processes)):
            if not self.processes[i].is_alive():
                all_alive = False

                # Clean up failed boat
                logging.info('Main Process: Boat {} is failed. Cleaning up ...'.format(i))
                self.processes[i].join()
                self.processes.pop(i)
                subprocess.call(['sh', 'reset.sh', str(i)])
                
                # Restart Clipper
                logging.info('Main Process: ... Clean up finished. Restarting Clipper for Boat {} ...'.format(i))
                if subprocess.call(['python3', 'clipper.py', str(i)]) == 0:
                    logging.info('Main Process: ... Clipper instances started.')
                else:
                    logging.error('Main Process: ... Error occurred when starting the Clipper instance. Please do it manually. A process for boat {} will be created anyway.'.format(i))

                # Restart boat process
                try:
                    logging.info('Main Process: Creating Boat {} ...'.format(i))
                    p = multiprocessing.Process(target=self.main, args=(i,))
                    p.start()
                    self.processes.insert(i, p)
                    logging.info('Main Process: ... Boat {} re-created and re-triggered.'.format(i))
                except:
                    logging.fatal('Main Process: Error occurred when restarting boats. Quitting.')
                    self.exit_and_cleanup()
        
        if all_alive:
            logging.error('Main Process: SIGCHLD received but all processes are alive.')
        
        # Set the handler back
        signal.signal(signal.SIGCHLD, self.sigchld_handler)

    def run(self):
        '''Blocking call to run the whole cluster'''

        # Start Clipper with a separate Python script, or Clipper does not work properly
        logging.info('Main Process: Starting Clipper instances ...')
        clipper_success = 0
        for i in range(self.num_nodes):
            clipper_success += subprocess.call(['python3', 'clipper.py', str(i)])
        if clipper_success == 0:
            logging.info('Main Process: ... Clipper instances started.')
        else:
            logging.fatal('Main Process: Error occurred on Clipper startup. Cleaning up.')
            for i in range(self.num_nodes):
                subprocess.call(['sh', 'reset.sh', str(i)])
            logging.info('Main Process: Cleanup finished. Bye-bye.')
            exit(1)

        # Setup signal handler for child process failure
        signal.signal(signal.SIGCHLD, self.sigchld_handler)

        # Multiprocessing
        try:
            for i in range(self.num_nodes):
                logging.info('Main Process: Creating Boat {} ...'.format(i))
                p = multiprocessing.Process(target=self.main, args=(i,))
                p.start()
                self.processes.append(p)
                logging.info('Main Process: ... Boat {} created and triggered.'.format(i))

        except:
            logging.fatal('Main Process: Error occurred when starting boats. Quitting.')
            self.exit_and_cleanup()

        # Running loop
        while True:
            try:
                signal.pause()
                # for p in self.processes:
                #     p.join()
                #     self.processes.remove(p)
            except KeyboardInterrupt:
                # Don't respond to child failure
                signal.signal(signal.SIGCHLD, signal.SIG_DFL)
                self.exit_and_cleanup()
            
            except BaseException as e:
                logging.error('Main Process: Unexpected exception thrown: {}'.format(e))

        # # For any exception (including KeyboardInterrupt)
        # finally:
            
            
    # Entry point for every process
    def main(self, node_id):
        boat = Boat(self.num_nodes, node_id, self.start_time)
        boat.run()

    def exit_and_cleanup(self):
        # Send SIGTERM to every process
        for p in self.processes:
            if p.is_alive():
                logging.info('Main Process: Terminating Boat {}. '.format(p))
                p.terminate()
            
        # Wait for processes to finish the cleanup
        while self.processes:
            for p in self.processes:
                try:
                    p.join()
                except AssertionError:
                    logging.error('Main Process: Process {} not joinable.'.format(p))
                self.processes.remove(p) 

        logging.info('Main Process: All boats terminated. Please wait for cleanup. This may take several minutes.')
        cleanup_success = 0
        for i in range(self.num_nodes):
            cleanup_success += subprocess.call(['sh', 'reset.sh', str(i)])
        if cleanup_success == 0:
            logging.info('Main Process: Cleanup finished. Bye-bye.')
        else:
            logging.error('Main Process: Error occurred on cleanup. Bye-bye anyway.')
        exit(0)