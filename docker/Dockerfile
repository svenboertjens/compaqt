# Use an argument to specify the Python version
ARG PYTHON_VERSION=3.11

# Base image for the specified Python version
FROM python:${PYTHON_VERSION}-slim

# Install system dependencies if needed (e.g., build tools for C extensions)
RUN apt-get update && apt-get install -y \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Copy your serializer and tests into the container
COPY compaqt /usr/src/app/compaqt
COPY tests /usr/src/app/tests
COPY setup.py /usr/src/app/setup.py
COPY pyproject.toml /usr/src/app/pyproject.toml
COPY README.md /usr/src/app/README.md

WORKDIR /usr/src/app

RUN pip install .
