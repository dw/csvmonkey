
import csv

writer = csv.writer(file('ram.noquotes-64mb.csv', 'w'), quoting=csv.QUOTE_NONE)
for row in csv.reader(file('ram.64mb.csv')):
    writer.writerow([c.replace(',', '') for c in row])
