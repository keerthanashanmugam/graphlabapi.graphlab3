#include <string>
#include <cstdlib>
#include <graphlab/util/timer.hpp>
#include <graphlab/util/memory_info.hpp>
#include <graphlab/comm/mpi_comm.hpp>
using namespace graphlab;

bool CHECK_COMM_RESULT = false;
int main(int argc, char** argv) {
  // make a small send window
  mpi_comm* comm = new mpi_comm(&argc, &argv, 
                                (size_t)1 * 1024 * 1024 * 1024);
  assert(comm->size() >= 2);
  if (comm->rank() == 0) std::cout << "barrier test.\n";
  timer ti; ti.start();
  for (size_t i = 0;i < 100; ++i) {
    comm->barrier();
  }
  if (comm->rank() == 0) { 
    std::cout << "Barrier in " 
              << ti.current_time_millis() / 100 << " ms" << std::endl;
  }

  // point
  if (comm->rank() == 0) std::cout << "point to point (0-1).\n";
  // create a bunch of arrays from 1 byte long, to 1 << MAXSEND bytes long
  // each send is issued 10 times and averaged
  size_t MIN_SEND = 4;
  size_t MAX_SEND = 24;
  char* c[MAX_SEND];

  size_t TOTAL_COMM = 64 * 1024 * 1024;

  if (comm->rank() == 0) {
    for (size_t i = MIN_SEND; i < MAX_SEND; ++i) {
      c[i] = (char*)malloc(1 << i);
      memset(c[i], i, 1 << i);
    }
  }

  comm->barrier();
  for (size_t i = MIN_SEND; i < MAX_SEND; ++i) {
    ti.start();
    size_t iterations = TOTAL_COMM / (1 << i);
    if (comm->rank() == 0) {
      for (size_t j = 0; j < iterations ; ++j) {
        comm->send(1, c[i], 1 << i);
      }
      double t = ti.current_time();
      std::cout << "Send of 64MB in " << (1<<i) << " chunks in " 
                << t << " s. "
                << "(" << TOTAL_COMM / t / 1024 / 1024 << " MBps)" << std::endl;
      comm->flush();
    } else if (comm->rank() == 1) {
      for (size_t j = 0; j < iterations ; ++j) {
        int source; size_t len;
        char* ret = NULL; 
        // spin on receive until I get 100
        while (ret == NULL) {
          ret = (char*)comm->receive(&source, &len);
          if (ret == NULL) usleep(100);
        }
        if ( CHECK_COMM_RESULT) {
          bool t = true;
          for (size_t k = 0; k < (1 << i); ++k) t &= (ret[k] == i);
          assert(t);
        }
        free(ret);
        assert(len == 1 << i);
      }
      double t = ti.current_time();
      std::cout << "Receive of 64MB in " << (1<<i) << " chunks in " 
                << t << " s. "
                << "(" << TOTAL_COMM / t / 1024 / 1024 << " MBps)" << std::endl;
    }
    //printf("%ld:%ld\n",comm->rank(),memory_info::rusage_maxrss());
    comm->barrier();
  }
  delete comm;
}
