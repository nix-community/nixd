# RUN: valgrind --leak-check=full --error-exitcode=1 nixd-ast-dump %s

# CHECK:
{
  a = 1;
  b = 2;
