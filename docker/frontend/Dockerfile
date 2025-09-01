FROM nginx:alpine

# Copy static frontend
COPY frontend/ /usr/share/nginx/html/

# Template + entrypoint to inject BACKEND_URL
COPY docker/frontend/nginx.conf.template /etc/nginx/templates/nginx.conf.template
COPY docker/frontend/docker-entrypoint.sh /docker-entrypoint.sh
RUN chmod +x /docker-entrypoint.sh

ENV BACKEND_URL=http://192.168.4.1

EXPOSE 80
ENTRYPOINT ["/docker-entrypoint.sh"]

