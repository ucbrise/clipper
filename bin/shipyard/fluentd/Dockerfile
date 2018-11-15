FROM fluent/fluentd:v0.12

RUN apk add --update --virtual .build-deps \
        sudo build-base ruby-dev \
 && sudo gem install fluent-plugin-kafka --no-document \
 && sudo gem sources --clear-all \
 && apk del .build-deps \
 && rm -rf /var/cache/apk/* \
           /home/fluent/.gem/ruby/2.3.0/cache/*.gem
COPY fluent.conf /fluentd/etc/fluent.conf
EXPOSE 24284
