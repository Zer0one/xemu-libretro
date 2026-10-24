"""Ideal double-precision comparison only; no GPU/FP32 or derivative validation."""
import math,json,random
random.seed(2947);cases=0;max_relative_error=0.
for scale in [(1,1),(2,2),(4,4),(4,2)]:
 for _ in range(1000):
  # Right triangles make the upstream calc_triz plane coefficients explicit.
  x0,y0=random.uniform(-500,500),random.uniform(-500,500)
  dx,dy=random.uniform(1,1000),random.uniform(1,1000)
  w=[10**random.uniform(-2,6) for _ in range(3)]
  a,b=random.random(),random.random()
  if a+b>1:a,b=1-a,1-b
  bc=[1-a-b,a,b]
  inv_w=sum(bc[i]/w[i] for i in range(3))
  old_bc=[bc[i]/w[i] for i in range(3)];norm=sum(old_bc)
  old_bc=[v/norm for v in old_bc]
  old_w=w[0]+old_bc[1]*(w[1]-w[0])+old_bc[2]*(w[2]-w[0])
  new_w=1/inv_w
  slope_x=(1/w[1]-1/w[0])/dx;slope_y=(1/w[2]-1/w[0])/dy
  old_slope=max(abs(slope_x),abs(slope_y))
  # Hardware derivatives are per host pixel; restore the guest-pixel units.
  new_slope=max(abs(slope_x/scale[0]*scale[0]),abs(slope_y/scale[1]*scale[1]))
  factor,offset=random.uniform(-.01,.01),random.uniform(-2,2)
  old=old_w+offset+factor*old_slope*old_w*old_w
  new=new_w+offset+factor*new_slope*new_w*new_w
  error=abs(new-old)/max(abs(old),1)
  assert math.isclose(old,new,rel_tol=1e-7,abs_tol=1e-7),(old,new)
  max_relative_error=max(max_relative_error,error);cases+=1
result={'triangle_samples':cases,'scales':[[1,1],[2,2],[4,4],[4,2]],'max_double_relative_error':max_relative_error,'boundary':'Ideal positive-W right-triangle math only. No GPU, FP32, real derivatives, clipping, or depth-quantization qualification.'}
print(json.dumps(result,indent=2))
