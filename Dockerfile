FROM capstone-c as c-compiler
FROM qemu-build

WORKDIR /app

COPY . ./caplifive-buildroot
COPY --from=c-compiler /app/ /app/capstone-c


RUN curl https://sh.rustup.rs -sSf | bash -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

RUN . $HOME/.cargo/env

ENV CAPSTONE_CC_PATH=/app/capstone-c
RUN cd caplifive-buildroot && make setup && make build CAPSTONE_CC_PATH=$CAPSTONE_CC_PATH
