    chmod +x *.sh && for file in *.c; do gcc "$file" -o "${file%.c}" -pthread; done && rm -rf *.c
