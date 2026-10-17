FROM python:3.12-slim
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1
WORKDIR /app
COPY aco_host /app/aco_host
COPY tests /app/tests
COPY build.py /app/build.py
RUN mkdir /data && chown 10001:10001 /data /app
USER 10001:10001
VOLUME ["/data"]
EXPOSE 27840
ENTRYPOINT ["python", "-m", "aco_host"]
CMD ["serve", "--data-dir", "/data", "--bind", "0.0.0.0", "--port", "27840"]
