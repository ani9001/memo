import sys
sys.path.append('/usr/local/lib/python3.7/site-packages')

import tweepy
import tweet_bot_token as token

auth = tweepy.OAuthHandler(token.consumer_key, token.consumer_secret)
auth.set_access_token(token.access_token, token.access_token_secret)
api = tweepy.API(auth)

if __name__ == '__main__':
 args = sys.argv
 if len(args) == 2:
  api.update_status(args[1])
