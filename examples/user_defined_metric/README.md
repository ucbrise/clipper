## User Define Metric (UDF) Example

This example demonstrate the ability to accept any user defined metrics in Clipper's monitoring system. From the user side, all the user need to do is

1. `import clipper_admin.metric as metric` to import the clipper.metric sub-package
2. `metric.add(metric_name, metric_type, metric_description, optional_histogram_bucket)` to add a metric.  The metric type is defined in [Prometheus Data Types](https://prometheus.io/docs/concepts/metric_types/)
3. `metric.report(metric_name, metric_data)` to report a metric. 

### Model and Query

In this example, we trained a tfidf word vectorizer and a logistic regression model in sklearn to classify [spam SMS messages](https://www.kaggle.com/uciml/sms-spam-collection-dataset). We sampled sentences in the [email spam and ham dataset](https://www.kaggle.com/c/adcg-ss14-challenge-02-spam-mails-detection) to query our model. We defined the following metrics:

```python
metric.add_metric('custom_vectorization_time_ms', 'Histogram', 'Time it takes to use tfidf transform',[0.1, 0.5, 0.8, 1.0, 1.2])

metric.add_metric('custom_lr_time_ms', 'Histogram', 'Time it takes to use logistic regression', [0.03, 0.05, 0.06, 0.1])

metric.add_metric('custom_choice_probability', 'Histogram', 'The logistic regressor probability output', [0.5, 0.7, 0.9, 1.0])

metric.add_metric('custom_spam_option_counter', 'Counter', 'The number of spam classified')

metric.add_metric('custom_ham_option_counter', 'Counter', 'The number of ham classfied')

metric.add_metric('custom_char_count', 'Histogram', 'The number of characters', [10, 50, 100, 300, 500, 800, 1200, 2000])

metric.add_metric('custom_word_count', 'Histogram', 'The number of words', [10, 50, 100, 150, 200])
```



### Run Demo

To run this demo, simply run:

1. `python2 query.py` to start clipper. The program will automatically query our model.
2. `python init_grana.py` to checkout some basic visualization of the user-defined-metrics. 