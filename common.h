// Authors: Michael Allen Jr | Wesley Carter
//          mma357           |  wc609
// Date: Oct 6, 2020


// A custom sequence counter data struct that makes
// server / client control easy by abstracting away
// sequence number management. Value is a circular list
// that only allows values from 0-7 and cycle keeps
// track of how many iterations the list has gone
// through so that distance can be calculated efficiently.
// This data structure serves as a bottleneck for the
// program as it only allows a maximum of ~34.3 bn
// packets to be sent. That limit is fine for this
// application, though.
struct SequenceCounter {
  unsigned int value;
  unsigned int cycle;

  SequenceCounter() {
    value = 0;
    cycle = 0;
  }

  // The ++ operator was overloaded to make
  // seqeunce numbers easy to manage while
  // automatically increasing cycle count.
  void operator++(int) {
    value = (value + 1) % 8;
    if (value == 0)
      ++cycle;
  }

  // Allows the sequence counter to be assigned
  // to a value while also incrementing the cycle
  // if necessary.
  SequenceCounter& operator=(int a) {
    if (a > 7) {
      ++cycle;
    }

    value = (a) % 8;
    return *this;
  }

  // Calculates distance using cycle and value.
  int distance(SequenceCounter &b) {
    if (cycle > b.cycle) {
      return (8 - b.value) + value;
    } else {
      return value - b.value;
    }
  }
};
