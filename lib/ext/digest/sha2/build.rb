extension do |e|
  e.name 'sha2'
  e.files '*.c'
  e.includes '.', '..'
  e.libs 'crypto'
end
