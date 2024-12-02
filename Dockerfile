FROM capstone-c as c-compiler
FROM qemu-build

WORKDIR /app

COPY . ./captainer-buildroot
COPY --from=c-compiler /app/ /app/capstone-c


RUN curl https://sh.rustup.rs -sSf | bash -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

RUN . $HOME/.cargo/env

ENV CAPSTONE_CC_PATH=/app/capstone-c/target/release
RUN cd captainer-buildroot && make setup && make build CAPSTONE_CC_PATH=$CAPSTONE_CC_PATH
