import sys

#https://stackoverflow.com/a/25597628/3027604
def get_averages(csv):
    column_sums = None
    with open(csv) as file:
        lines = file.readlines()
        rows_of_numbers = [map(float, line.split(',')) for line in lines]
        sums = map(sum, zip(*rows_of_numbers))
        averages = [sum_item / len(lines) for sum_item in sums]
        return averages

folders = ["output5", "output12", "output19", "output26", "output33", "output40", "output47"]
files = ["metricsClassic.csv", "deepdecodedClassic.csv", "interpolationMetricsdecodedClassic.csv", "metricsAllKey.csv",  "deepdecodedAllKey.csv", "interpolationMetricsdecodedAllKey.csv", "metrics.csv", "deepdecoded.csv", "interpolationMetricsdecoded.csv"]
for file in files:
	print(file)
	for folder in folders:
		path=sys.argv[1]+"/"+folder+"/"+file
		res = get_averages(path)
		print(str(res[1])+"\t"+str(res[0]))
