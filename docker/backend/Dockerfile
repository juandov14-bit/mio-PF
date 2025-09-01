FROM node:18-alpine
WORKDIR /app
COPY docker/backend/package.json ./
RUN npm install --production
COPY docker/backend/server.js ./
EXPOSE 3000
CMD ["npm", "start"]

