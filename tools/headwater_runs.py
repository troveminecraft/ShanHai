"""Longest upstream channel distance, rebuilt after all graph reconnects."""
import math,collections
def bake_headwater_runs(j):
 segments=j['rivers']['segments'];incoming=collections.Counter(tuple(s[2:4]) for s in segments)
 outgoing=collections.defaultdict(list);nodes=set(incoming)
 for i,s in enumerate(segments):
  a,b=tuple(s[:2]),tuple(s[2:4]);nodes.add(a);outgoing[a].append((b,math.dist(a,b)))
 distance={p:0. for p in nodes if not incoming[p]};queue=collections.deque(distance)
 visited=0
 while queue:
  a=queue.popleft();visited+=1
  for b,length in outgoing[a]:
   distance[b]=max(distance.get(b,0.),distance[a]+length);incoming[b]-=1
   if not incoming[b]:queue.append(b)
 if visited!=len(nodes):raise ValueError('Cyclic drainage graph')
 j['rivers']['headwaterRuns']=[[distance[tuple(s[:2])],distance[tuple(s[2:4])]] for s in segments]
 return dict(headwaters=sum(v==0 for v in distance.values()),segments=len(segments))
