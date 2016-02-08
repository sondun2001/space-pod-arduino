template <size_t CAPACITY>
class BufferedPrint : public Print {
 public:
  BufferedPrint(Print& destination) : _destination(destination), _size(0) {}

  ~BufferedPrint() { flush(); }

  virtual size_t write(uint8_t c) {
    _buffer[_size++] = c;

    if (_size + 1 == CAPACITY) {
      flush();
    }
  }

  void flush() {
    buffer[_size] = '\0';
    _destination.print(_buffer);
    _size = 0;
  }

 private:
  Print& _destination;
  size_t _size;
  char _buffer[CAPACITY];
};