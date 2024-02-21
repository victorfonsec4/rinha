FROM library/postgres:latest

COPY postgresql.conf /etc/postgresql/postgresql.conf

ENV POSTGRES_HOST_AUTH_METHOD=trust

COPY ./init-scripts/ /docker-entrypoint-initdb.d/
EXPOSE 5432

CMD ["postgres", "-c", "max_connections=100", "-c", "fsync=off", "-c", "shared_buffers=75MB", "-c", "effective_cache_size=225MB", "-c", "maintenance_work_mem=19200kB", "-c", "checkpoint_completion_target=0.9", "-c", "wal_buffers=2304kB", "-c", "default_statistics_target=100", "-c", "random_page_cost=1.1", "-c", "effective_io_concurrency=200", "-c", "work_mem=480kB", "-c", "huge_pages=off", "-c", "min_wal_size=2GB", "-c", "max_wal_size=8GB", "-c", "max_worker_processes=4", "-c", "max_parallel_workers_per_gather=2", "-c", "max_parallel_workers=4", "-c", "max_parallel_maintenance_workers=2", "-c", "unix_socket_directories=/tmp,/var/run/postgresql"]
