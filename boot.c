
char hex_to_char(int _x) {
  volatile register int x = _x;
  return (x < 10 ? '0' + x : 'A' + (x - 10));
}